/*
	Improved FFT and IFFT UGens for SuperCollider 3
	Copyright (c) 2007 Dan Stowell, incorporating much code from
	SuperCollider 3 Copyright (c) 2002 James McCartney.
	All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "FFT_UGens.h"

// We include vDSP even if not using for FFT, since we want to use some vectorised add/mul tricks
#if SC_DARWIN
#include "vecLib/vDSP.h"
#endif


// These specify the min & max FFT sizes expected (used when creating windows, also allocating some other arrays).
#define SC_FFT_MINSIZE 8
#define SC_FFT_LOG2_MINSIZE 3
#define SC_FFT_MAXSIZE 8192
#define SC_FFT_LOG2_MAXSIZE 13
// Note that things like *fftWindow actually allow for other sizes, to be created on user request.
#define SC_FFT_ABSOLUTE_MAXSIZE 2147483648
#define SC_FFT_LOG2_ABSOLUTE_MAXSIZE 31
#define SC_FFT_LOG2_ABSOLUTE_MAXSIZE_PLUS1 32


// Decisions here about which FFT library to use - vDSP only exists on Mac BTW.
// We include the relevant libs but also ensure that one, and only one, of them is active...
#if !SC_FFT_FFTW && SC_DARWIN
	#define SC_FFT_FFTW 0
	#define SC_FFT_VDSP 1
#else        
//#elif SC_FFT_FFTW
	#define SC_FFT_FFTW 1
	#define SC_FFT_VDSP 0
	#include <fftw3.h>
#endif

extern InterfaceTable *ft;

// These values are referred to from SC lang as well as in the following code - do not rearrange!
const int WINDOW_RECT = -1;
const int WINDOW_WELCH = 0;
const int WINDOW_HANN = 1;

static float *fftWindow[2][SC_FFT_LOG2_ABSOLUTE_MAXSIZE_PLUS1];

#if SC_FFT_VDSP
static FFTSetup fftSetup[SC_FFT_LOG2_ABSOLUTE_MAXSIZE_PLUS1]; // vDSP setups, one per FFT size
static COMPLEX_SPLIT splitBuf; // Temp buf for holding rearranged data
#endif

struct FFTBase : public Unit
{
	SndBuf *m_fftsndbuf;
	float *m_fftbuf;
	
	int m_pos, m_fullbufsize, m_audiosize; // "fullbufsize" includes any zero-padding, "audiosize" does not.
	int m_log2n_full, m_log2n_audio;
	
	uint32 m_fftbufnum;
	
#if SC_FFT_FFTW
	fftwf_plan m_plan;
#endif
	float *m_transformbuf;
	int m_hopsize, m_shuntsize; // These add up to m_audiosize
	int m_wintype;
};

struct FFT : public FFTBase
{
	float *m_inbuf; 
};

struct IFFT : public FFTBase
{
	float *m_olabuf;
	float m_scalefactor; // Different FFT algorithms introduce different scaling through ->FFT->IFFT->
};

struct FFTTrigger : public FFTBase
{
	int m_numPeriods, m_periodsRemain, m_polar;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

extern "C"
{
	void FFT_Ctor(FFT* unit);
	void FFT_ClearUnitOutputs(FFT *unit, int wrongNumSamples);
	void FFT_next(FFT *unit, int inNumSamples);
	void FFT_Dtor(FFT* unit);

	void IFFT_Ctor(IFFT* unit);
	void IFFT_next(IFFT *unit, int inNumSamples);
	void IFFT_Dtor(IFFT* unit);
	
	void FFTTrigger_Ctor(FFTTrigger* unit);
	void FFTTrigger_next(FFTTrigger* unit, int inNumSamples);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

int ensure_fftwindow(FFTBase *unit);

void DoWindowing(FFTBase *unit, float* data);
void DoWindowing(FFTBase *unit, float* data)
{
	int audiosize = unit->m_audiosize;
	int fullbufsize = unit->m_fullbufsize;
	int i;
	if(unit->m_wintype != WINDOW_RECT){
		float *win = fftWindow[unit->m_wintype][unit->m_log2n_audio];
		if (!win) return;
		win--;
	#if SC_DARWIN
		vDSP_vmul(data, 1, win+1, 1, data, 1, audiosize);
	#else
		float *in = data - 1;
		for (i=0; i< audiosize ; ++i) {
			*++in *= *++win;
		}
	#endif

	#if SC_FFT_VDSP
		// vDSP scales the data differently during the forward FFT transform, compared against Green or FFTW, so we need to rescale
		float scalefac = 0.5f;
		vDSP_vsmul(data, 1, &scalefac, data, 1, audiosize); // i.e. multiply all in-place by scalefac
	#endif
	}
	// Zero-padding:
	memset(data + audiosize, 0, (fullbufsize - audiosize) * sizeof(float));
}

int FFTBase_Ctor(FFTBase *unit, int frmsizinput);
int FFTBase_Ctor(FFTBase *unit, int frmsizinput)
{
	World *world = unit->mWorld;

	uint32 bufnum = (uint32)ZIN0(0);
	//Print("FFTBase_Ctor: bufnum is %i\n", bufnum);
	if (bufnum >= world->mNumSndBufs) bufnum = 0;
	SndBuf *buf = world->mSndBufs + bufnum; 
	
	unit->m_fftsndbuf = buf;
	unit->m_fftbufnum = bufnum;
	unit->m_fullbufsize = buf->samples;
	int framesize = (int)ZIN0(frmsizinput);
	if(framesize < 1)
		unit->m_audiosize = buf->samples;
	else
		unit->m_audiosize = sc_min(buf->samples, framesize);

	unit->m_log2n_full  = LOG2CEIL(unit->m_fullbufsize);
	unit->m_log2n_audio = LOG2CEIL(unit->m_audiosize);

	// Although FFTW allows non-power-of-two buffers (vDSP doesn't), this would complicate the windowing, so we don't allow it.
	if (!ISPOWEROFTWO(unit->m_fullbufsize)) {
		Print("FFTBase_Ctor error: buffer size (%i) not a power of two.\n", unit->m_fullbufsize);
		return 0;
	}
	else if (!ISPOWEROFTWO(unit->m_audiosize)) {
		Print("FFTBase_Ctor error: audio frame size (%i) not a power of two.\n", unit->m_audiosize);
		return 0;
	}
	else if (unit->m_audiosize < SC_FFT_MINSIZE || 
			(((int)(unit->m_audiosize / unit->mWorld->mFullRate.mBufLength)) 
					* unit->mWorld->mFullRate.mBufLength != unit->m_audiosize)) {
		Print("FFTBase_Ctor error: audio frame size (%i) not a multiple of the block size (%i).\n", unit->m_audiosize, unit->mWorld->mFullRate.mBufLength);
		return 0;
	}
	else if (unit->m_fullbufsize > SC_FFT_MAXSIZE){
#if 0
		if(unit->mWorld->mVerbosity > -2)
			Print("FFTBase_Ctor error: buffer size (%i) larger than hard-coded upper limit for FFT UGen (%i).\n", unit->m_fullbufsize, SC_FFT_MAXSIZE);
		return 0;
#else
		// Buffer is larger than the range of sizes we provide for at startup; we can get ready just-in-time though
		ensure_fftwindow(unit);
#endif
	}
	
	unit->m_pos = 0;
	
	ZOUT0(0) = ZIN0(0);
	
	return 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void FFT_Ctor(FFT *unit)
{
	unit->m_wintype = (int)ZIN0(3); // wintype may be used by the base ctor
	if(!FFTBase_Ctor(unit, 5)){
		SETCALC(FFT_ClearUnitOutputs);
		return;
	}
	int fullbufsize = unit->m_fullbufsize * sizeof(float);
	int audiosize = unit->m_audiosize * sizeof(float);
	
	int hopsize = (int)(sc_max(sc_min(ZIN0(2), 1.f), 0.f) * unit->m_audiosize);
	if (((int)(hopsize / unit->mWorld->mFullRate.mBufLength)) * unit->mWorld->mFullRate.mBufLength 
				!= hopsize) {
		Print("FFT_Ctor: hopsize (%i) not an exact multiple of SC's block size (%i) - automatically corrected.\n", hopsize, unit->mWorld->mFullRate.mBufLength);
		hopsize = ((int)(hopsize / unit->mWorld->mFullRate.mBufLength)) * unit->mWorld->mFullRate.mBufLength;
	}
	unit->m_hopsize = hopsize;
	unit->m_shuntsize = unit->m_audiosize - hopsize;
	
	unit->m_inbuf = (float*)RTAlloc(unit->mWorld, audiosize);
	
#if SC_FFT_FFTW
	if(unit->mWorld->mVerbosity > 1)
		Print("FFT using FFTW\n");
	// Transform buf is two floats "too big" because of FFTWF's output ordering
	unit->m_transformbuf = (float*)RTAlloc(unit->mWorld, fullbufsize + sizeof(float) + sizeof(float));
	unit->m_plan = fftwf_plan_dft_r2c_1d(unit->m_fullbufsize, unit->m_transformbuf, 
			(fftwf_complex*) unit->m_transformbuf, FFTW_ESTIMATE);
	memset(unit->m_transformbuf, 0, fullbufsize + sizeof(float) + sizeof(float));
#else
	if(unit->mWorld->mVerbosity > 1)
		Print("FFT using vDSP\n");
	// vDSP packs the nyquist in with the DC, so size is same as input buffer (plus zeropadding)
	unit->m_transformbuf = (float*)RTAlloc(unit->mWorld, fullbufsize);
	memset(unit->m_transformbuf, 0, fullbufsize);
#endif
	
	memset(unit->m_inbuf, 0, audiosize);
	
	//Print("FFT_Ctor: hopsize %i, shuntsize %i, bufsize %i, wintype %i, \n",
	//	unit->m_hopsize, unit->m_shuntsize, unit->m_bufsize, unit->m_wintype);
	
	SETCALC(FFT_next);	
}

void FFT_Dtor(FFT *unit)
{
#if SC_FFT_FFTW
	fftwf_destroy_plan(unit->m_plan);
#endif
	
	RTFree(unit->mWorld, unit->m_inbuf);
	RTFree(unit->mWorld, unit->m_transformbuf);
}

// Ordinary ClearUnitOutputs outputs zero, potentially telling the IFFT (+ PV UGens) to act on buffer zero, so let's skip that:
void FFT_ClearUnitOutputs(FFT *unit, int wrongNumSamples){
	ZOUT0(0) = -1;
}

void FFT_next(FFT *unit, int wrongNumSamples)
{
	float *in = IN(1);
	float *out = unit->m_inbuf + unit->m_pos + unit->m_shuntsize;
	
	int numSamples = unit->mWorld->mFullRate.mBufLength;
	
	// copy input
	//Copy(numSamples, out, in);
	memcpy(out, in, numSamples * sizeof(float));
	
	unit->m_pos += numSamples;
	
	bool gate = ZIN0(4) > 0.f; // Buffer shunting continues, but no FFTing
	
	if (unit->m_pos != unit->m_hopsize || unit->m_fftsndbuf->samples != unit->m_fullbufsize) {
		ZOUT0(0) = -1.f;
	} else {
		
		unit->m_fftbuf = unit->m_fftsndbuf->data;

		unit->m_pos = 0;		
		
		if(gate){
			int audiosize = unit->m_audiosize * sizeof(float);
			int fullbufsize = unit->m_fullbufsize * sizeof(float);
			
			// Data goes to transform buf
			memcpy(unit->m_transformbuf, unit->m_inbuf, audiosize);
			
			DoWindowing(unit, unit->m_transformbuf);

#if SC_FFT_FFTW
			fftwf_execute(unit->m_plan);
			// Rearrange output data onto public buffer
			memcpy(unit->m_fftbuf, unit->m_transformbuf, fullbufsize);
			unit->m_fftbuf[1] = unit->m_transformbuf[unit->m_fullbufsize]; // Pack nyquist val in
#else
			// Perform even-odd split
			vDSP_ctoz((COMPLEX*) unit->m_transformbuf, 2, &splitBuf, 1, unit->m_fullbufsize >> 1);
			// Now the actual FFT
			vDSP_fft_zrip(fftSetup[unit->m_log2n_full], &splitBuf, 1, unit->m_log2n_full, FFT_FORWARD);
			// Copy the data to the public output buf, transforming it back out of "split" representation
			vDSP_ztoc(&splitBuf, 1, (DSPComplex*) unit->m_fftbuf, 2, unit->m_fullbufsize >> 1);
#endif
			
			unit->m_fftsndbuf->coord = coord_Complex;
			ZOUT0(0) = unit->m_fftbufnum;
		} else {
			ZOUT0(0) = -1;
		}
		// Shunt input buf down
		memcpy(unit->m_inbuf, unit->m_inbuf + unit->m_hopsize, unit->m_shuntsize * sizeof(float));
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////

void IFFT_Ctor(IFFT* unit){
	unit->m_wintype = (int)ZIN0(1); // wintype may be used by the base ctor
	if(!FFTBase_Ctor(unit, 2)){
		SETCALC(*ClearUnitOutputs);
		return;
	}
	
	// This will hold the transformed and progressively overlap-added data ready for outputting.
	unit->m_olabuf = (float*)RTAlloc(unit->mWorld, unit->m_audiosize * sizeof(float));
	memset(unit->m_olabuf, 0, unit->m_audiosize * sizeof(float));
	
	
#if SC_FFT_FFTW
	// Transform buf is two floats "too big" because of FFTWF's output ordering
	unit->m_transformbuf = (float*)RTAlloc(unit->mWorld, (unit->m_fullbufsize+2) * sizeof(float));
	// We'll be going from m_transformbuf back to fftbuf
	unit->m_plan = fftwf_plan_dft_c2r_1d(unit->m_fullbufsize, (fftwf_complex*) unit->m_transformbuf, 
			unit->m_fftsndbuf->data, FFTW_ESTIMATE);

	memset(unit->m_transformbuf, 0, (unit->m_fullbufsize+2) * sizeof(float));
	unit->m_scalefactor = 1.f / unit->m_fullbufsize;
#else
	unit->m_scalefactor = 2.f / unit->m_fullbufsize;
#endif
	
	// "pos" will be reset to zero when each frame comes in. Until then, the following ensures silent output at first:
	unit->m_pos = unit->m_audiosize;
	
	SETCALC(IFFT_next);	
}

void IFFT_Dtor(IFFT* unit){
	RTFree(unit->mWorld, unit->m_olabuf);
#if SC_FFT_FFTW
	fftwf_destroy_plan(unit->m_plan);
	RTFree(unit->mWorld, unit->m_transformbuf);
#endif
}

void IFFT_next(IFFT *unit, int wrongNumSamples){
	
#if SC_DARWIN
	float *out = OUT(0);
#else
	float *out = ZOUT(0);
#endif

	// Load state from struct into local scope
	int pos     = unit->m_pos;
	int fullbufsize  = unit->m_fullbufsize;
	int audiosize = unit->m_audiosize;
	int numSamples = unit->mWorld->mFullRate.mBufLength;
	float *olabuf = unit->m_olabuf;
	float fbufnum = ZIN0(0);
	float scalefactor = unit->m_scalefactor;
	
	// Only run the IFFT if we're receiving a new block of input data - otherwise just output data already received
	if (fbufnum >= 0.f){
		
		// Ensure it's in cartesian format, not polar
		ToComplexApx(unit->m_fftsndbuf);
		
		float* fftbuf = unit->m_fftsndbuf->data;
		
		// Do IFFT "in-place"... or however the different FFT libraries would like us to...
#if SC_FFT_FFTW
		// For FFTW we can't do it in-place because it expects a slightly different data format
		float *transformbuf = unit->m_transformbuf;
		memcpy(transformbuf, fftbuf, fullbufsize * sizeof(float));
		transformbuf[1] = 0.f;
		transformbuf[unit->m_fullbufsize] = fftbuf[1];  // Nyquist goes all the way down to the end of the line...
		transformbuf[unit->m_fullbufsize+1] = 0.f;
		fftwf_execute(unit->m_plan);
#else
		// For vDSP we can't really do it in place either,
		//  because the data needs to be converted into then out of "split" (odd/even) representation
		vDSP_ctoz((COMPLEX*) fftbuf, 2, &splitBuf, 1, unit->m_fullbufsize >> 1);
		vDSP_fft_zrip(fftSetup[unit->m_log2n_full], &splitBuf, 1, unit->m_log2n_full, FFT_INVERSE);
		vDSP_ztoc(&splitBuf, 1, (DSPComplex*) fftbuf, 2, unit->m_fullbufsize >> 1);
#endif		
		
		// Then apply window in-place
		DoWindowing(unit, fftbuf);
		
		// Then shunt the "old" time-domain output down by one hop
		int hopsamps = pos;
		int shuntsamps = audiosize - hopsamps;
		if(hopsamps != audiosize){  // There's only copying to be done if the position isn't all the way to the end of the buffer
			memcpy(olabuf, olabuf+hopsamps, shuntsamps * sizeof(float));
		}
		
		// Then mix the "new" time-domain data in - adding at first, then just setting (copying) where the "old" is supposed to be zero.
		// NB we re-use the "pos" variable temporarily here for write rather than read
		for(pos = 0; pos < shuntsamps; ++pos){
			olabuf[pos] += fftbuf[pos];
		}
		memcpy(olabuf + pos, fftbuf + pos, (audiosize-pos) * sizeof(float));
		
		// Move the pointer back to zero, which is where playback will next begin
		pos = 0;
		
	} // End of has-the-chain-fired
	
	// Now we can output some stuff, as long as there is still data waiting to be output.
	// If there is NOT data waiting to be output, we output zero. (Either irregular/negative-overlap 
	//     FFT firing, or FFT has given up, or at very start of execution.)
	if(pos >= audiosize){
		ClearUnitOutputs(unit, numSamples);
	}else{
#if SC_DARWIN
	vDSP_vsmul(olabuf + pos, 1, &scalefactor, out, 1, numSamples);
	pos += numSamples;
#else
		for(int i=0; i<numSamples; ++i){
			ZXP(out) = (*(olabuf + (pos++))) * scalefactor;
		}
#endif
	}
	unit->m_pos = pos;
	
}



/////////////////////////////////////////////////////////////////////////////////////////////

float* create_fftwindow(int wintype, int log2n);
float* create_fftwindow(int wintype, int log2n)
{
	int size = 1 << log2n;
	float *win = (float*)malloc(size * sizeof(float));
	
	double winc;
	switch(wintype){
		case WINDOW_WELCH:
			winc = pi / size;
			for (int i=0; i<size; ++i) {
				double w = i * winc;
				win[i] = sin(w);
			}
			break;
		case WINDOW_HANN:
			winc = twopi / size;
			for (int i=0; i<size; ++i) {
				double w = i * winc;
				win[i] = 0.5 - 0.5 * cos(w);
			}
			break;
	}
	
	return win;
	
}

// Called by FFT_Base_Ctor - will automatically expand the available range of FFT sizes if neccessary.
// Note that the expansion, if triggered, will cause a performance hit as things are malloc'ed, realloc'ed, etc.
static int largest_log2n = SC_FFT_LOG2_MAXSIZE;
int ensure_fftwindow(FFTBase *unit){
	
	int log2n_audio = unit->m_log2n_audio;
	int log2n_full = unit->m_log2n_full;
	
	// Ensure we have enough space to do our calcs
	if(log2n_full > largest_log2n){
		largest_log2n = log2n_full;
#if SC_FFT_VDSP
		size_t newsize = (1 << largest_log2n) * sizeof(float) / 2;
		splitBuf.realp = (float*) realloc (splitBuf.realp, newsize);
		splitBuf.imagp = (float*) realloc (splitBuf.imagp, newsize);
#endif
	}
	
	// Ensure our window has been created
	if((unit->m_wintype != -1) && (fftWindow[unit->m_wintype][log2n_audio] == 0)){
		if(unit->mWorld->mVerbosity > 0)
			Print("ensure_fftwindow(): creating and mallocing window of size %i, type %i, in real-time thread.\n", unit->m_audiosize, unit->m_wintype);
		fftWindow[unit->m_wintype][log2n_audio] = create_fftwindow(unit->m_wintype, log2n_audio);
	}
	
	// Ensure our FFT twiddle factors (or whatever) have been created
#if SC_FFT_VDSP
	if(fftSetup[log2n_full] == 0)
		fftSetup[log2n_full] = vDSP_create_fftsetup (log2n_full, FFT_RADIX2);
#else
		// Nothing needed here for FFTW
#endif
	
	return 1;
}

void init_ffts();
void init_ffts()
{
	for (int wintype=0; wintype<2; ++wintype) {
		for (int i=0; i< SC_FFT_LOG2_ABSOLUTE_MAXSIZE_PLUS1; ++i) {
			fftWindow[wintype][i] = 0;
		}
		for (int i= SC_FFT_LOG2_MINSIZE; i < SC_FFT_LOG2_MAXSIZE+1; ++i) {
			fftWindow[wintype][i] = create_fftwindow(wintype, i);
		}
	}
#if SC_FFT_VDSP
	// vDSP inits its twiddle factors
	for (int i= SC_FFT_LOG2_MINSIZE; i < SC_FFT_LOG2_MAXSIZE+1; ++i) {
		fftSetup[i] = vDSP_create_fftsetup (i, FFT_RADIX2);
		if(fftSetup[i] == NULL){
			Print("FFT ERROR: Mac vDSP library could not allocate FFT setup for size %i\n", 1<<i);
		}
	}
	// vDSP prepares its memory-aligned buffer for rearranging input data.
	// Note max size here - meaning max input buffer size is these two sizes added together.
	// vec_malloc used in API docs, but apparently that's deprecated and malloc is sufficient for aligned memory on OSX.
	splitBuf.realp = (float*) malloc ( SC_FFT_MAXSIZE * sizeof(float) / 2);
	splitBuf.imagp = (float*) malloc ( SC_FFT_MAXSIZE * sizeof(float) / 2);
	//Print("FFT init: vDSP initialised.\n");
#else
	//Print("FFT init: FFTW, no init needed.\n");
#endif
}

void FFTTrigger_Ctor(FFTTrigger *unit)
{
	World *world = unit->mWorld;

	uint32 bufnum = (uint32)IN0(0);
	//Print("FFTBase_Ctor: bufnum is %i\n", bufnum);
	if (bufnum >= world->mNumSndBufs) bufnum = 0;
	SndBuf *buf = world->mSndBufs + bufnum; 
	
	unit->m_fftsndbuf = buf;
	unit->m_fftbufnum = bufnum;
	unit->m_fullbufsize = buf->samples;

	int numSamples = unit->mWorld->mFullRate.mBufLength;
	float dataHopSize = IN0(1);
	int initPolar = unit->m_polar = (int)IN0(2);
	unit->m_numPeriods = unit->m_periodsRemain = (int)(((float)unit->m_fullbufsize * dataHopSize) / numSamples) - 1; 
		
	buf->coord = (IN0(2) == 1.f) ? coord_Polar : coord_Complex;
	    
	OUT0(0) = IN0(0);
	SETCALC(FFTTrigger_next);
}

void FFTTrigger_next(FFTTrigger *unit, int inNumSamples)
{	
	if (unit->m_periodsRemain > 0) {
		ZOUT0(0) = -1.f;
		unit->m_periodsRemain--;
	} else {
		ZOUT0(0) = unit->m_fftbufnum;
		unit->m_pos = 0;
		unit->m_periodsRemain = unit->m_numPeriods;		
	}
		
}

void init_SCComplex(InterfaceTable *inTable);

void initFFT(InterfaceTable *inTable)
{
	ft = inTable;

	init_ffts();

	DefineDtorUnit(FFT);
	DefineDtorUnit(IFFT);
	DefineSimpleUnit(FFTTrigger);
}

