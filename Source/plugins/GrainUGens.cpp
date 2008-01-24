/*
 *  JoshUGens.cpp
 *  xSC3plugins
 *
 *  Created by Josh Parmenter on 2/4/05.
 *  Copyright 2005 __MyCompanyName__. All rights reserved.
 *
 */
/*
	SuperCollider real time audio synthesis system
    Copyright (c) 2002 James McCartney. All rights reserved.
	http://www.audiosynth.com

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


#include "SC_PlugIn.h"

// macros to put rgen state in registers
#define RGET \
	RGen& rgen = *unit->mParent->mRGen; \
	uint32 s1 = rgen.s1; \
	uint32 s2 = rgen.s2; \
	uint32 s3 = rgen.s3; 

#define RPUT \
	rgen.s1 = s1; \
	rgen.s2 = s2; \
	rgen.s3 = s3;

static InterfaceTable *ft;

const int kMaxGrains = 64;

const int kMaxSynthGrains = 512;

struct GrainInG
{
	double b1, y1, y2, curamp, winPos, winInc; // envelope
	int counter, chan;
	float pan1, pan2, winType;
};

struct GrainIn : public Unit
{
	int mNumActive, m_channels;
	float curtrig;
	GrainInG mGrains[kMaxSynthGrains];
};

struct GrainSinG
{
	double b1, y1, y2, curamp, winPos, winInc; // envelope
	int counter, chan;
	float pan1, pan2, winType;
	int32 oscphase; // the phase of the osc inside this grain
	int32 freq; // the freq of the osc inside this grain in phase inc
};

struct GrainSin : public Unit
{
	int mNumActive, m_channels;
	uint32 m_lomask;
	float curtrig;
	double m_cpstoinc, m_radtoinc;
	GrainSinG mGrains[kMaxSynthGrains];
};

struct GrainFMG
{
	int32 coscphase, moscphase; // the phase of the osc inside this grain
	int32 mfreq; // the freq of the osc inside this grain in phase inc
	double b1, y1, y2, curamp, winPos, winInc; // envelope
	float deviation, carbase, pan1, pan2, winType;
	int counter, chan;
};


struct GrainFM : public Unit
{
	int mNumActive, m_channels;
	uint32 m_lomask;
	float curtrig;
	double m_cpstoinc, m_radtoinc;
	GrainFMG mGrains[kMaxSynthGrains];
};

struct GrainBufG
{
	double phase, rate;
	double b1, y1, y2, curamp, winPos, winInc; // envelope
	float pan1, pan2, winType;
	int counter, chan, bufnum, interp;
};

struct GrainBuf : public Unit
{
	int mNumActive, m_channels;
	float curtrig;
	GrainBufG mGrains[kMaxGrains];
};

struct WarpWinGrain
{
	double phase, rate;
	double winPos, winInc, b1, y1, y2, curamp; // tells the grain where to look in the winBuf for an amp value
	int counter, bufnum, interp;
	float winType;
};

struct Warp1 : public Unit
{
	int m_fbufnum;
	SndBuf* m_buf;
	int mNumActive[16];
	int mNextGrain[16];
	WarpWinGrain mGrains[16][kMaxGrains];
};
//////////// end add //////////////////////

////////////////////////////////////////////////////////////////////////

extern "C"
{
	void load(InterfaceTable *inTable);

	void GrainIn_Ctor(GrainIn* unit);
	void GrainIn_next_a(GrainIn *unit, int inNumSamples);
	void GrainIn_next_k(GrainIn *unit, int inNumSamples);	

	void GrainSin_Ctor(GrainSin* unit);
	void GrainSin_next_a(GrainSin *unit, int inNumSamples);
	void GrainSin_next_k(GrainSin *unit, int inNumSamples);
	
	void GrainFM_Ctor(GrainFM* unit);
	void GrainFM_next_a(GrainFM *unit, int inNumSamples);
	void GrainFM_next_k(GrainFM *unit, int inNumSamples);
	
	void GrainBuf_Ctor(GrainBuf* unit);
	void GrainBuf_next_a(GrainBuf *unit, int inNumSamples);
	void GrainBuf_next_k(GrainBuf *unit, int inNumSamples);

	void Warp1_next(Warp1 *unit, int inNumSamples);
	void Warp1_Ctor(Warp1* unit);
		
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Granular UGens ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

static float cubicinterp(float x, float y0, float y1, float y2, float y3)
{
	// 4-point, 3rd-order Hermite (x-form)
	float c0 = y1;
	float c1 = 0.5f * (y2 - y0);
	float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
	float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

	return ((c3 * x + c2) * x + c1) * x + c0;
}
inline float IN_AT(Unit* unit, int index, int offset) 
{
	if (INRATE(index) == calc_FullRate) return IN(index)[offset];
	if (INRATE(index) == calc_DemandRate) return DEMANDINPUT_A(index, offset + 1);
	return ZIN0(index);
}

inline double sc_gloop(double in, double hi) 
{
	// avoid the divide if possible
	if (in >= hi) {
		in -= hi;
		if (in < hi) return in;
	} else if (in < 0.) {
		in += hi;
		if (in >= 0.) return in;
	} else return in;
	
	return in - hi * floor(in/hi); 
}

#define GRAIN_BUF \
	SndBuf *buf = bufs + bufnum; \
	float *bufData __attribute__((__unused__)) = buf->data; \
	uint32 bufChannels __attribute__((__unused__)) = buf->channels; \
	uint32 bufSamples __attribute__((__unused__)) = buf->samples; \
	uint32 bufFrames = buf->frames; \
	int guardFrame __attribute__((__unused__)) = bufFrames - 2; \

#define CHECK_BUF \
	if (!bufData) { \
                unit->mDone = true; \
		ClearUnitOutputs(unit, inNumSamples); \
		return; \
	}

#define GET_GRAIN_WIN \
		window = unit->mWorld->mSndBufs + (int)winType; \
		windowData = window->data; \
		windowSamples = window->samples; \
		windowFrames = window->frames; \
		windowGuardFrame = windowFrames - 1; \

#define GRAIN_LOOP_BODY_4 \
		float amp = y1 * y1; \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase; \
		float* table0 = table1 - 1; \
		float* table2 = table1 + 1; \
		float* table3 = table1 + 2; \
		if (iphase == 0) { \
			table0 += bufSamples; \
		} else if (iphase >= guardFrame) { \
			if (iphase == guardFrame) { \
				table3 -= bufSamples; \
			} else { \
				table2 -= bufSamples; \
				table3 -= bufSamples; \
			} \
		} \
		float fracphase = phase - (double)iphase; \
		float a = table0[0]; \
		float b = table1[0]; \
		float c = table2[0]; \
		float d = table3[0]; \
		float outval = amp * cubicinterp(fracphase, a, b, c, d); \
		ZXP(out1) += outval; \
		double y0 = b1 * y1 - y2; \
		y2 = y1; \
		y1 = y0; \


#define GRAIN_LOOP_BODY_2 \
		float amp = y1 * y1; \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase; \
		float* table2 = table1 + 1; \
		if (iphase > guardFrame) { \
			table2 -= bufSamples; \
		} \
		float fracphase = phase - (double)iphase; \
		float b = table1[0]; \
		float c = table2[0]; \
		float outval = amp * (b + fracphase * (c - b)); \
		ZXP(out1) += outval; \
		double y0 = b1 * y1 - y2; \
		y2 = y1; \
		y1 = y0; \
		

#define GRAIN_LOOP_BODY_1 \
		float amp = y1 * y1; \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float outval = amp * bufData[iphase]; \
		ZXP(out1) += outval; \
		double y0 = b1 * y1 - y2; \
		y2 = y1; \
		y1 = y0; \

#define BUF_GRAIN_AMP \
		winPos += winInc; \
		int iWinPos = (int)winPos; \
		double winFrac = winPos - (double)iWinPos;\
		float* winTable1 = windowData + iWinPos;\
		float* winTable2 = winTable1 + 1;\
		if (winPos > windowGuardFrame) {\
		    winTable2 -= windowSamples; \
		    } \
		amp = lininterp(winFrac, winTable1[0], winTable2[0]); \

#define GET_INTERP_GRAIN_WIN \
		SndBuf *windowA = unit->mWorld->mSndBufs + (int)grain->mWindowA; \
		float *windowDataA __attribute__((__unused__)) = windowA->data; \
		uint32 windowSamplesA __attribute__((__unused__)) = windowA->samples; \
		uint32 windowFramesA = windowA->frames; \
		int windowGuardFrameA __attribute__((__unused__)) = windowFramesA - 1; \
		SndBuf *windowB = unit->mWorld->mSndBufs + (int)grain->mWindowB; \
		float *windowDataB __attribute__((__unused__)) = windowB->data; \
		uint32 windowSamplesB __attribute__((__unused__)) = windowB->samples; \
		uint32 windowFramesB = windowB->frames; \
		int windowGuardFrameB __attribute__((__unused__)) = windowFramesB - 1; \
				
#define BUF_INTERP_GRAIN_AMP \
		winPosA += winIncA; \
		int iWinPosA = (int)winPosA; \
		double winFracA = winPosA - (double)iWinPosA;\
		float* winTableA1 = windowDataA + iWinPosA;\
		float* winTableA2 = winTableA1 + 1;\
		if (winPosA > windowGuardFrameA) {\
		    winTableA2 -= windowSamplesA; \
		    } \
		float ampA = lininterp(winFracA, winTableA1[0], winTableA2[0]); \
		winPosB += winIncB; \
		int iWinPosB = (int)winPosB; \
		double winFracB = winPosB - (double)iWinPosB;\
		float* winTableB1 = windowDataB + iWinPosB;\
		float* winTableB2 = winTableB1 + 1;\
		if (winPosB > windowGuardFrameB) {\
		    winTableB2 -= windowSamplesB; \
		    } \
		float ampB = lininterp(winFracB, winTableB1[0], winTableB2[0]); \
		amp = lininterp(grain->ifac, ampA, ampB);\
		
#define BUF_GRAIN_LOOP_BODY_4 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase; \
		float* table0 = table1 - 1; \
		float* table2 = table1 + 1; \
		float* table3 = table1 + 2; \
		if (iphase == 0) { \
			table0 += bufSamples; \
		} else if (iphase >= guardFrame) { \
			if (iphase == guardFrame) { \
				table3 -= bufSamples; \
			} else { \
				table2 -= bufSamples; \
				table3 -= bufSamples; \
			} \
		} \
		float fracphase = phase - (double)iphase; \
		float a = table0[0]; \
		float b = table1[0]; \
		float c = table2[0]; \
		float d = table3[0]; \
		float outval = amp * cubicinterp(fracphase, a, b, c, d); \
		ZXP(out1) += outval; \

#define BUF_GRAIN_LOOP_BODY_2 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase; \
		float* table2 = table1 + 1; \
		if (iphase > guardFrame) { \
			table2 -= bufSamples; \
		} \
		float fracphase = phase - (double)iphase; \
		float b = table1[0]; \
		float c = table2[0]; \
		float outval = amp * (b + fracphase * (c - b)); \
		ZXP(out1) += outval; \
		
// amp needs to be calculated by looking up values in window

#define BUF_GRAIN_LOOP_BODY_1 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float outval = amp * bufData[iphase]; \
		ZXP(out1) += outval; \


#define SETUP_OUT \
	uint32 numOutputs = unit->mNumOutputs; \
	if (numOutputs > bufChannels) { \
                unit->mDone = true; \
		ClearUnitOutputs(unit, inNumSamples); \
		return; \
	} \
	float *out[16]; \
	for (uint32 i=0; i<numOutputs; ++i) out[i] = ZOUT(i); 
	
// for standard SC dist ///
#define SETUP_GRAIN_OUTS \
	uint32 numOutputs = unit->mNumOutputs;\
	float *out[16]; \
	for (uint32 i=0; i<numOutputs; ++i) out[i] = OUT(i); \

#define CALC_GRAIN_PAN \
	float panangle, pan1, pan2; \
	float *out1, *out2; \
	if (numOutputs > 1) { \
		if (numOutputs == 2) pan = pan * 0.5; \
		pan = sc_wrap(pan * 0.5f, 0.f, 1.f); \
		float cpan = numOutputs * pan + 0.5; \
		float ipan = floor(cpan); \
		float panfrac = cpan - ipan; \
		panangle = panfrac * pi2; \
		grain->chan = (int)ipan; \
		if (grain->chan >= (int)numOutputs) grain->chan -= numOutputs; \
		pan1 = grain->pan1 = cos(panangle); \
		pan2 = grain->pan2 = sin(panangle); \
	} else { \
		grain->chan = 0; \
		pan1 = grain->pan1 = 1.; \
		pan2 = grain->pan2 = 0.; \
	}
	
#define GET_GRAIN_INIT_AMP \
	if(grain->winType < 0.){ \
	    w = pi / counter; \
	    b1 = grain->b1 = 2. * cos(w); \
	    y1 = sin(w); \
	    y2 = 0.; \
	    amp = y1 * y1; \
	    } else { \
	    amp = windowData[0]; \
	    winPos = grain->winPos = 0.f; \
	    winInc = grain->winInc = (double)windowSamples / counter; \
	    } \

#define CALC_NEXT_GRAIN_AMP \
	if(grain->winType < 0.){ \
	    y0 = b1 * y1 - y2; \
	    y2 = y1; \
	    y1 = y0; \
	    amp = y1 * y1; \
	    } else { \
	    winPos += winInc; \
	    int iWinPos = (int)winPos; \
	    double winFrac = winPos - (double)iWinPos; \
	    float* winTable1 = windowData + iWinPos; \
	    float* winTable2 = winTable1 + 1; \
	    if (!windowData) break; \
	    if (winPos > windowGuardFrame) { \
		winTable2 -= windowSamples; \
		} \
	    amp = lininterp(winFrac, winTable1[0], winTable2[0]); \
	    } \
	    
#define GET_GRAIN_AMP_PARAMS \
	if(grain->winType < 0.){ \
	    b1 = grain->b1; \
	    y1 = grain->y1; \
	    y2 = grain->y2; \
	    amp = grain->curamp; \
	    } else { \
	    window = unit->mWorld->mSndBufs + (int)grain->winType; \
	    windowData = window->data; \
	    windowSamples = window->samples; \
	    windowFrames = window->frames; \
	    windowGuardFrame = windowFrames - 1; \
	    if (!windowData) break; \
	    winPos = grain->winPos; \
	    winInc = grain->winInc; \
	    amp = grain->curamp; \
	    } \
	    
#define SAVE_GRAIN_AMP_PARAMS \
	grain->y1 = y1; \
	grain->y2 = y2; \
	grain->winPos = winPos; \
	grain->winInc = winInc; \
	grain->curamp = amp; \
	grain->counter -= nsmps; \

#define WRAP_CHAN \
	out1 = out[grain->chan] + i; \
	if(numOutputs > 1) { \
	    if((grain->chan + 1) >= (int)numOutputs) \
		out2 = out[0] + i; \
		else \
		out2 = out[grain->chan + 1] + i; \
	    } \

#define WRAP_CHAN_K \
	out1 = out[grain->chan]; \
	if(numOutputs > 1) { \
	    if((grain->chan + 1) >= (int)numOutputs) \
		out2 = out[0]; \
		else \
		out2 = out[grain->chan + 1]; \
	    } \
	    
#define GET_PAN_PARAMS \
	float pan1 = grain->pan1; \
	uint32 chan1 = grain->chan; \
	float *out1 = out[chan1]; \
	if(numOutputs > 1){ \
	    pan2 = grain->pan2; \
	    uint32 chan2 = chan1 + 1; \
	    if (chan2 >= numOutputs) chan2 = 0; \
	    out2 = out[chan2]; \
	    } \

#define WARP_GRAIN_LOOP_BODY_4 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase * bufChannels; \
		float* table0 = table1 - bufChannels; \
		float* table2 = table1 + bufChannels; \
		float* table3 = table2 + bufChannels; \
		if (iphase == 0) { \
			table0 += bufSamples; \
		} else if (iphase >= guardFrame) { \
			if (iphase == guardFrame) { \
				table3 -= bufSamples; \
			} else { \
				table2 -= bufSamples; \
				table3 -= bufSamples; \
			} \
		} \
		float fracphase = phase - (double)iphase; \
		float a = table0[n]; \
		float b = table1[n]; \
		float c = table2[n]; \
		float d = table3[n]; \
		float outval = amp * cubicinterp(fracphase, a, b, c, d); \
		ZXP(out1) += outval; \

#define WARP_GRAIN_LOOP_BODY_2 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase * bufChannels; \
		float* table2 = table1 + bufChannels; \
		if (iphase > guardFrame) { \
			table2 -= bufSamples; \
		} \
		float fracphase = phase - (double)iphase; \
		float b = table1[n]; \
		float c = table2[n]; \
		float outval = amp * (b + fracphase * (c - b)); \
		ZXP(out1) += outval; \
		
// amp needs to be calculated by looking up values in window

#define WARP_GRAIN_LOOP_BODY_1 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float outval = amp * bufData[iphase + n]; \
		ZXP(out1) += outval; \

//////////////////// InGrain ////////////////////

void GrainIn_next_a(GrainIn *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	//begin add
	SETUP_GRAIN_OUTS
	// end add
	float *trig = IN(0);
	float *in = IN(2);
	float winSize;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;
	

	for (int i=0; i < unit->mNumActive; ) {
		GrainInG *grain = unit->mGrains + i;
		GET_GRAIN_AMP_PARAMS
		// begin add //
		
		float pan2 = 0.f;
		float *out2;
		
		GET_PAN_PARAMS
		// end add //
		int nsmps = sc_min(grain->counter, inNumSamples);
		for (int j=0; j<nsmps; ++j) {
		    float outval = amp * in[j];
		    // begin change / add //
		    out1[j] += outval * pan1;
		    if(numOutputs > 1) out2[j] += outval * pan2;
		    // end change //
		    
		    CALC_NEXT_GRAIN_AMP
		    
		    }
		    
		SAVE_GRAIN_AMP_PARAMS

		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	}
	
	for (int i=0; i<inNumSamples; ++i) {
		if ((unit->curtrig <= 0) && (trig[i] > 0.0)) { 
			// start a grain
			if (unit->mNumActive+1 >= kMaxSynthGrains) {Print("Too many grains!\n"); break;}
			float winType = IN_AT(unit, 4, i);
			GET_GRAIN_WIN
			if((windowData) || (winType < 0.)) {
			    GrainInG *grain = unit->mGrains + unit->mNumActive++;
			    winSize = IN_AT(unit, 1, i);
			    double counter = winSize * SAMPLERATE;
			    counter = sc_max(4., counter);
			    grain->counter = (int)counter;
			    grain->winType = winType;
			    
			    GET_GRAIN_INIT_AMP
				
			    float *in1 = in + i;
			    // begin add //
			    float pan = IN_AT(unit, 3, i);
			    
			    CALC_GRAIN_PAN
			    
			    WRAP_CHAN
				
			    // end add //
			    
			    int nsmps = sc_min(grain->counter, inNumSamples - i);
			    for (int j=0; j<nsmps; ++j) {
				float outval = amp * in1[j];
				// begin add / change
				out1[j] += outval * pan1;
				if(numOutputs > 1) out2[j] += outval * pan2;
				// end add / change
				CALC_NEXT_GRAIN_AMP
			    }

			    SAVE_GRAIN_AMP_PARAMS

			    if (grain->counter <= 0) {
				    // remove grain
				    *grain = unit->mGrains[--unit->mNumActive];
			    }
			}
		    } 
		unit->curtrig = trig[i];
	}	
}

void GrainIn_next_k(GrainIn *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	// begin add //
	SETUP_GRAIN_OUTS
	// end add
	float trig = IN0(0);
	float *in = IN(2);
	float winSize;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;
	
	for (int i=0; i < unit->mNumActive; ) {
		GrainInG *grain = unit->mGrains + i;

		GET_GRAIN_AMP_PARAMS

		// begin add //
		float pan2 = 0.;
		float *out2;
		
		GET_PAN_PARAMS
		// end add //
		
		int nsmps = sc_min(grain->counter, inNumSamples);
		for (int j=0; j<nsmps; ++j) {
		    float outval = amp * in[j];
		    // begin change / add //
		    out1[j] += outval * pan1;
		    if(numOutputs > 1) out2[j] += outval * pan2;
		    // end change //
		    CALC_NEXT_GRAIN_AMP

		    }
		    
		SAVE_GRAIN_AMP_PARAMS

		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	}
	
	if ((unit->curtrig <= 0) && (trig > 0.0)) { 
		    // start a grain
		    if (unit->mNumActive+1 >= kMaxSynthGrains) 
		    {
		    Print("Too many grains!\n");
		    } else {
		    float winType = IN0(4);
		    GET_GRAIN_WIN
		    if((windowData) || (winType < 0.)) {
			GrainInG *grain = unit->mGrains + unit->mNumActive++;
			winSize = IN0(1);
			double counter = winSize * SAMPLERATE;
			counter = sc_max(4., counter);
			grain->counter = (int)counter;
			grain->winType = winType;
			
			GET_GRAIN_INIT_AMP
			// begin add
			float pan = IN0(3);
			
			CALC_GRAIN_PAN
			
			WRAP_CHAN_K

			// end add
			int nsmps = sc_min(grain->counter, inNumSamples);
			for (int j=0; j<nsmps; ++j) {
			    float outval = amp * in[j];
			    // begin add / change
			    out1[j] += outval * pan1;
			    if(numOutputs > 1) out2[j] += outval * pan2;
			    // end add / change
			    CALC_NEXT_GRAIN_AMP

			}
			
			SAVE_GRAIN_AMP_PARAMS

			if (grain->counter <= 0) {
				// remove grain
				*grain = unit->mGrains[--unit->mNumActive];
			}
		    }
		}
	    }
	    unit->curtrig = trig;
}


void GrainIn_Ctor(GrainIn *unit)
{	
	
	if (INRATE(0) == calc_FullRate) 
	    SETCALC(GrainIn_next_a);
	    else
	    SETCALC(GrainIn_next_k);
	unit->mNumActive = 0;
	unit->curtrig = 0.f;
	GrainIn_next_k(unit, 1);
}

void GrainSin_next_a(GrainSin *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	
	SETUP_GRAIN_OUTS
	        
	float *trig = IN(0);
	float winSize, freq;
	float *table0 = ft->mSineWavetable;
	float *table1 = table0 + 1;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;

	for (int i=0; i < unit->mNumActive; ) {
		GrainSinG *grain = unit->mGrains + i;
		GET_GRAIN_AMP_PARAMS

		// begin add //
		
		float pan2 = 0.f;
		float *out2;

		GET_PAN_PARAMS

		// end add //

		int32 thisfreq = grain->freq;
		int32 oscphase = grain->oscphase;
		
		int nsmps = sc_min(grain->counter, inNumSamples);
		for (int j=0; j<nsmps; ++j) {
		    float outval = amp * lookupi1(table0, table1, oscphase, unit->m_lomask);
		    // begin change / add //
		    out1[j] += outval * pan1;
		    if(numOutputs > 1) out2[j] += outval * pan2;
		    // end change //
		    CALC_NEXT_GRAIN_AMP

		    oscphase += thisfreq;
		    }
		    
		SAVE_GRAIN_AMP_PARAMS
		
		grain->oscphase = oscphase;
		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	}
	
	for (int i=0; i<inNumSamples; ++i) {
		if ((unit->curtrig <= 0) && (trig[i] > 0.0)) { 
			// start a grain
			if (unit->mNumActive+1 >= kMaxSynthGrains) {Print("Too many grains!\n"); break;}
			float winType = IN_AT(unit, 4, i);
			GET_GRAIN_WIN
			if((windowData) || (winType < 0.)) {
			    GrainSinG *grain = unit->mGrains + unit->mNumActive++;
			    // INRATE(1) == calcFullRate
			    freq = IN_AT(unit, 2, i);
			    winSize = IN_AT(unit, 1, i);
			    int32 thisfreq = grain->freq = (int32)(unit->m_cpstoinc * freq);
			    int32 oscphase = 0;
			    double counter = winSize * SAMPLERATE;
			    counter = sc_max(4., counter);
			    grain->counter = (int)counter;
			    grain->winType = winType; //IN_AT(unit, 4, i);
			    
			    GET_GRAIN_INIT_AMP

			    // begin add //
			    float pan = IN_AT(unit, 3, i);
			    
			    CALC_GRAIN_PAN

			    WRAP_CHAN

			    // end add //
	    
			    int nsmps = sc_min(grain->counter, inNumSamples - i);
			    for (int j=0; j<nsmps; ++j) {
				float outval = amp * lookupi1(table0, table1, oscphase, unit->m_lomask); 
				// begin add / change
				out1[j] += outval * pan1;
				if(numOutputs > 1) out2[j] += outval * pan2;
				// end add / change
				CALC_NEXT_GRAIN_AMP

				oscphase += thisfreq;
			    }
			    grain->oscphase = oscphase;

			    SAVE_GRAIN_AMP_PARAMS

			    if (grain->counter <= 0) {
				    // remove grain
				    *grain = unit->mGrains[--unit->mNumActive];
			    }
			}
		    } 
		unit->curtrig = trig[i];
	}	
}

void GrainSin_next_k(GrainSin *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	// begin add //
	SETUP_GRAIN_OUTS
	// end add
	float trig = IN0(0);
	float winSize, freq;
	float *table0 = ft->mSineWavetable;
	float *table1 = table0 + 1;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;
	

	for (int i=0; i < unit->mNumActive; ) {
		GrainSinG *grain = unit->mGrains + i;

		GET_GRAIN_AMP_PARAMS

		// begin add //
		float pan2 = 0.;
		float *out2;
		
		GET_PAN_PARAMS

		// end add //
		int32 thisfreq = grain->freq;
		int32 oscphase = grain->oscphase;
		
		int nsmps = sc_min(grain->counter, inNumSamples);
		for (int j=0; j<nsmps; ++j) {
		    float outval = amp * lookupi1(table0, table1, oscphase, unit->m_lomask);
		    // begin change / add //
		    out1[j] += outval * pan1;
		    if(numOutputs > 1) out2[j] += outval * pan2;
		    // end change //

		    CALC_NEXT_GRAIN_AMP

		    oscphase += thisfreq;
		    }
		    
		SAVE_GRAIN_AMP_PARAMS
		
		grain->oscphase = oscphase;
		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	}
	
	if ((unit->curtrig <= 0) && (trig > 0.0)) { 
		    // start a grain
		    if (unit->mNumActive+1 >= kMaxSynthGrains) 
		    {
		    Print("Too many grains!\n");
		    } else {
		    float winType = IN0(4);
		    GET_GRAIN_WIN
		    if((windowData) || (winType < 0.)) {
			GrainSinG *grain = unit->mGrains + unit->mNumActive++;
			freq = IN0(2);
			winSize = IN0(1);
			int32 thisfreq = grain->freq = (int32)(unit->m_cpstoinc * freq);
			int32 oscphase = 0;
			double counter = winSize * SAMPLERATE;
			counter = sc_max(4., counter);
			grain->counter = (int)counter;
			grain->winType = winType;
			
			GET_GRAIN_INIT_AMP

			// begin add
			float pan = IN0(3);
			
			CALC_GRAIN_PAN
			
			WRAP_CHAN_K

			// end add
			int nsmps = sc_min(grain->counter, inNumSamples);
			for (int j=0; j<nsmps; ++j) {
			    float outval = amp * lookupi1(table0, table1, oscphase, unit->m_lomask); 
			    // begin add / change
			    out1[j] += outval * pan1;
			    if(numOutputs > 1) out2[j] += outval * pan2;
			    // end add / change
			    CALC_NEXT_GRAIN_AMP

			    oscphase += thisfreq;
			}
			grain->oscphase = oscphase;

			SAVE_GRAIN_AMP_PARAMS

			if (grain->counter <= 0) {
				// remove grain
				*grain = unit->mGrains[--unit->mNumActive];
			}
		    } 
		}
	    }
	    unit->curtrig = trig;
}


void GrainSin_Ctor(GrainSin *unit)
{	
	
	if (INRATE(0) == calc_FullRate) 
	    SETCALC(GrainSin_next_a);
	    else
	    SETCALC(GrainSin_next_k);
	int tableSizeSin = ft->mSineSize;
	unit->m_lomask = (tableSizeSin - 1) << 3; 
	unit->m_radtoinc = tableSizeSin * (rtwopi * 65536.); 
	unit->m_cpstoinc = tableSizeSin * SAMPLEDUR * 65536.;   
	unit->curtrig = 0.f;
	unit->mNumActive = 0;
	GrainSin_next_k(unit, 1);
}


void GrainFM_next_a(GrainFM *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	//begin add
	SETUP_GRAIN_OUTS
	// end add
	float *trig = IN(0);
	float winSize, carfreq, modfreq, index;
	
	float *table0 = ft->mSineWavetable;
	float *table1 = table0 + 1;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;
	
	for (int i=0; i < unit->mNumActive; ) {
		GrainFMG *grain = unit->mGrains + i;

		GET_GRAIN_AMP_PARAMS

		int32 mfreq = grain->mfreq;
		int32 moscphase = grain->moscphase;
		int32 coscphase = grain->coscphase;
		float deviation = grain->deviation;
		float carbase = grain->carbase;
		// begin add //
		
		float pan2 = 0.f;
		float *out2;
		
		GET_PAN_PARAMS

		// end add //
		int nsmps = sc_min(grain->counter, inNumSamples);
		for (int j=0; j<nsmps; ++j) {
			float thismod = lookupi1(table0, table1, moscphase, unit->m_lomask) * deviation;
			float outval = amp * lookupi1(table0, table1, coscphase, unit->m_lomask); 
			// begin change / add //
			out1[j] += outval * pan1;
			if(numOutputs > 1) out2[j] += outval * pan2;
			// end change //
			CALC_NEXT_GRAIN_AMP

			int32 cfreq = (int32)(unit->m_cpstoinc * (carbase + thismod)); // needs to be calced in the loop!
			coscphase += cfreq;
			moscphase += mfreq;
		    } // need to save float carbase, int32 mfreq, float deviation
		grain->coscphase = coscphase;
		grain->moscphase = moscphase;

		SAVE_GRAIN_AMP_PARAMS

		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	}
	
	for (int i=0; i<inNumSamples; ++i) {
	    if ((unit->curtrig <= 0) && (trig[i] > 0.0)) { 
		// start a grain
		if (unit->mNumActive+1 >= kMaxSynthGrains) {Print("Too many grains!\n"); break;}
		float winType = IN_AT(unit, 6, i);
		GET_GRAIN_WIN
		if((windowData) || (winType < 0.)) {
		    
		    GrainFMG *grain = unit->mGrains + unit->mNumActive++;
		    winSize = IN_AT(unit, 1, i);
		    carfreq = IN_AT(unit, 2, i);
		    modfreq = IN_AT(unit, 3, i);
		    index = IN_AT(unit, 4, i);
		    float deviation = grain->deviation = index * modfreq;
		    int32 mfreq = grain->mfreq = (int32)(unit->m_cpstoinc * modfreq);
		    grain->carbase = carfreq;
		    int32 coscphase = 0;
		    int32 moscphase = 0;
		    double counter = winSize * SAMPLERATE;
		    counter = sc_max(4., counter);
		    grain->counter = (int)counter;
		    grain->winType = winType; //IN_AT(unit, 6, i);
		    
		    GET_GRAIN_INIT_AMP

		    // begin add //
		    float pan = IN_AT(unit, 5, i);
		    
		    CALC_GRAIN_PAN

		    WRAP_CHAN

			
		    // end add //
		    int nsmps = sc_min(grain->counter, inNumSamples - i);
		    for (int j=0; j<nsmps; ++j) {
			float thismod = lookupi1(table0, table1, moscphase, unit->m_lomask) * deviation;
			float outval = amp * lookupi1(table0, table1, coscphase, unit->m_lomask); 
			// begin add / change
			out1[j] += outval * pan1;
			if(numOutputs > 1) out2[j] += outval * pan2;
			// end add / change
			CALC_NEXT_GRAIN_AMP

			int32 cfreq = (int32)(unit->m_cpstoinc * (carfreq + thismod)); // needs to be calced in the loop!
			coscphase += cfreq;
			moscphase += mfreq;
		    } // need to save float carbase, int32 mfreq, float deviation
		    grain->coscphase = coscphase;
		    grain->moscphase = moscphase;
		    
		    SAVE_GRAIN_AMP_PARAMS

		    if (grain->counter <= 0) {
			    // remove grain
			    *grain = unit->mGrains[--unit->mNumActive];
		    }
		} 
		}
		unit->curtrig = trig[i];
	}	
}

void GrainFM_next_k(GrainFM *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	// begin add //
	SETUP_GRAIN_OUTS
	// end add
	float trig = IN0(0);
	float winSize, carfreq, modfreq, index;
	
	float *table0 = ft->mSineWavetable;
	float *table1 = table0 + 1;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;
	
	for (int i=0; i < unit->mNumActive; ) {
		GrainFMG *grain = unit->mGrains + i;

		GET_GRAIN_AMP_PARAMS

		int32 mfreq = grain->mfreq;
		int32 moscphase = grain->moscphase;
		int32 coscphase = grain->coscphase;
		float deviation = grain->deviation;
		float carbase = grain->carbase;

		// begin add //
		float pan2;
		float *out2;
		
		GET_PAN_PARAMS
		
		// end add //
		
		int nsmps = sc_min(grain->counter, inNumSamples);
		for (int j=0; j<nsmps; ++j) {
			float thismod = lookupi1(table0, table1, moscphase, unit->m_lomask) * deviation;
			float outval = amp * lookupi1(table0, table1, coscphase, unit->m_lomask); 
			// begin change / add //
			out1[j] += outval * pan1;
			if(numOutputs > 1) out2[j] += outval * pan2;
			// end change //
			CALC_NEXT_GRAIN_AMP

			int32 cfreq = (int32)(unit->m_cpstoinc * (carbase + thismod)); // needs to be calced in the loop!
			coscphase += cfreq;
			moscphase += mfreq;
		    } // need to save float carbase, int32 mfreq, float deviation
		grain->coscphase = coscphase;
		grain->moscphase = moscphase;
		SAVE_GRAIN_AMP_PARAMS

		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	    }
	
	    if ((unit->curtrig <= 0) && (trig > 0.0)) { 
		// start a grain
		if (unit->mNumActive+1 >= kMaxSynthGrains) {Print("Too many grains!\n");
		} else {
		float winType = IN0(6);
		GET_GRAIN_WIN
		if((windowData) || (winType < 0.)) {
		    GrainFMG *grain = unit->mGrains + unit->mNumActive++;
		    winSize = IN0(1);
		    carfreq = IN0(2);
		    modfreq = IN0(3);
		    index = IN0(4);
		    float deviation = grain->deviation = index * modfreq;
		    int32 mfreq = grain->mfreq = (int32)(unit->m_cpstoinc * modfreq);
		    grain->carbase = carfreq;
		    int32 coscphase = 0;
		    int32 moscphase = 0;
		    double counter = winSize * SAMPLERATE;
		    counter = sc_max(4., counter);
		    grain->counter = (int)counter;
		    grain->winType = winType;
		    
		    GET_GRAIN_INIT_AMP
		    // begin add
		    float pan = IN0(5);
		    
		    CALC_GRAIN_PAN
		    
		    WRAP_CHAN_K

		    // end add
		    int nsmps = sc_min(grain->counter, inNumSamples);
		    for (int j=0; j<nsmps; ++j) {
			float thismod = lookupi1(table0, table1, moscphase, unit->m_lomask) * deviation;
			float outval = amp * lookupi1(table0, table1, coscphase, unit->m_lomask); 
			// begin add / change
			out1[j] += outval * pan1;
			if(numOutputs > 1) out2[j] += outval * pan2;
			// end add / change
			CALC_NEXT_GRAIN_AMP

			int32 cfreq = (int32)(unit->m_cpstoinc * (carfreq + thismod)); // needs to be calced in the loop!
			coscphase += cfreq;
			moscphase += mfreq;
		    } // need to save float carbase, int32 mfreq, float deviation
		    grain->coscphase = coscphase;
		    grain->moscphase = moscphase;
		    SAVE_GRAIN_AMP_PARAMS

		    if (grain->counter <= 0) {
			    // remove grain
			    *grain = unit->mGrains[--unit->mNumActive];
		    }
		} 
	    }
	}
	unit->curtrig = trig;
}

void GrainFM_Ctor(GrainFM *unit)
{	
	if (INRATE(0) == calc_FullRate) 
	    SETCALC(GrainFM_next_a);
	    else
	    SETCALC(GrainFM_next_k);
	int tableSizeSin = ft->mSineSize;
	unit->m_lomask = (tableSizeSin - 1) << 3; 
	unit->m_radtoinc = tableSizeSin * (rtwopi * 65536.); 
	unit->m_cpstoinc = tableSizeSin * SAMPLEDUR * 65536.;   
	unit->curtrig = 0.f;	
	unit->mNumActive = 0;
	GrainFM_next_k(unit, 1);
}

#define GRAIN_BUF_LOOP_BODY_4 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase; \
		float* table0 = table1 - 1; \
		float* table2 = table1 + 1; \
		float* table3 = table1 + 2; \
		if (iphase == 0) { \
			table0 += bufSamples; \
		} else if (iphase >= guardFrame) { \
			if (iphase == guardFrame) { \
				table3 -= bufSamples; \
			} else { \
				table2 -= bufSamples; \
				table3 -= bufSamples; \
			} \
		} \
		float fracphase = phase - (double)iphase; \
		float a = table0[0]; \
		float b = table1[0]; \
		float c = table2[0]; \
		float d = table3[0]; \
		float outval = amp * cubicinterp(fracphase, a, b, c, d); \
		out1[j] += outval * pan1; \
		if(numOutputs > 1) out2[j] += outval * pan2; \


#define GRAIN_BUF_LOOP_BODY_2 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase; \
		float* table2 = table1 + 1; \
		if (iphase > guardFrame) { \
			table2 -= bufSamples; \
		} \
		float fracphase = phase - (double)iphase; \
		float b = table1[0]; \
		float c = table2[0]; \
		float outval = amp * (b + fracphase * (c - b)); \
		out1[j] += outval * pan1; \
		if(numOutputs > 1) out2[j] += outval * pan2; \
		

#define GRAIN_BUF_LOOP_BODY_1 \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float outval = amp * bufData[iphase]; \
		out1[j] += outval * pan1; \
		if(numOutputs > 1) out2[j] += outval * pan2; \



void GrainBuf_next_a(GrainBuf *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	//begin add
	SETUP_GRAIN_OUTS
	// end add
	float *trig = IN(0);
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;	
	World *world = unit->mWorld;
	SndBuf *bufs = world->mSndBufs;
	uint32 numBufs = world->mNumSndBufs;
	
	for (int i=0; i < unit->mNumActive; ) {
		GrainBufG *grain = unit->mGrains + i;
		uint32 bufnum = grain->bufnum;
		
		GRAIN_BUF
		
		if (bufChannels != 1) {
			 ++i;
			 continue;
		}
		double loopMax = (double)bufFrames;
		double rate = grain->rate;
		double phase = grain->phase;

		GET_GRAIN_AMP_PARAMS
		
		// begin add //
		float pan2 = 0.f;; 
		float *out2;
		GET_PAN_PARAMS
		
		// end add //
		int nsmps = sc_min(grain->counter, inNumSamples);
		if (grain->interp >= 4) {
			for (int j=0; j<nsmps; j++) {
				GRAIN_BUF_LOOP_BODY_4
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		} else if (grain->interp >= 2) {
			for (int j=0; j<nsmps; j++) {
				GRAIN_BUF_LOOP_BODY_2
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		} else {
			for (int j=0; j<nsmps; j++) {
				GRAIN_BUF_LOOP_BODY_1
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		}
		
		grain->phase = phase;

		SAVE_GRAIN_AMP_PARAMS

		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	}
	
	for (int i=0; i<inNumSamples; i++) {
		if ((trig[i] > 0) && (unit->curtrig <=0)) {
			// start a grain
			if (unit->mNumActive+1 >= kMaxSynthGrains) {
			Print("Too many grains!\n");
			} else {
			float winType = IN_AT(unit, 7, i);
			if(winType >= 0.) { 
			    GET_GRAIN_WIN
			    }
			if((windowData) || (winType < 0.)) {
				uint32 bufnum = (uint32)IN_AT(unit, 2, i);
				if (bufnum >= numBufs) continue;
				
				GRAIN_BUF
				
				if (bufChannels != 1) continue;

				float bufSampleRate = buf->samplerate;
				float bufRateScale = bufSampleRate * SAMPLEDUR;
				double loopMax = (double)bufFrames;

				GrainBufG *grain = unit->mGrains + unit->mNumActive++;
				grain->bufnum = bufnum;
				
				double counter = IN_AT(unit, 1, i) * SAMPLERATE;
				counter = sc_max(4., counter);
				grain->counter = (int)counter;
				grain->winType = winType;
				
				GET_GRAIN_INIT_AMP

				double rate = grain->rate = IN_AT(unit, 3, i) * bufRateScale;
				double phase = IN_AT(unit, 4, i) * bufFrames;
				grain->interp = (int)IN_AT(unit, 5, i);

				// begin add //
				float pan = IN_AT(unit, 6, i);
				
				CALC_GRAIN_PAN

				WRAP_CHAN

				// end add //
					
				int nsmps = sc_min(grain->counter, inNumSamples-i);
				if (grain->interp >= 4) {
					for (int j=0; j<nsmps; j++) {
						GRAIN_BUF_LOOP_BODY_4
						CALC_NEXT_GRAIN_AMP
						phase += rate;
					}
				} else if (grain->interp >= 2) {
					for (int j=0; j<nsmps; j++) {
						GRAIN_BUF_LOOP_BODY_2
						CALC_NEXT_GRAIN_AMP
						phase += rate;
					}
				} else {
					for (int j=0; j<nsmps; j++) {
						GRAIN_BUF_LOOP_BODY_1
						CALC_NEXT_GRAIN_AMP
						phase += rate;
					}
				}
				
				grain->phase = phase;
				
				SAVE_GRAIN_AMP_PARAMS
				
				if (grain->counter <= 0) {
					// remove grain
					*grain = unit->mGrains[--unit->mNumActive];
				}
			    }
		    }
		}
		unit->curtrig = trig[i];
	    }
	
}

void GrainBuf_next_k(GrainBuf *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	//begin add
	SETUP_GRAIN_OUTS

	float trig = IN0(0);
	
	World *world = unit->mWorld;
	SndBuf *bufs = world->mSndBufs;
	//uint32 numBufs = world->mNumSndBufs;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;
		
	for (int i=0; i < unit->mNumActive; ) {
		GrainBufG *grain = unit->mGrains + i;
		uint32 bufnum = grain->bufnum;
		
		GRAIN_BUF
		
		if (bufChannels != 1) {
			 ++i;
			 continue;
		}

		double loopMax = (double)bufFrames;

		double rate = grain->rate;
		double phase = grain->phase;
		
		GET_GRAIN_AMP_PARAMS
		
		// begin add //
		float pan2 = 0.f;
		float *out2;
		GET_PAN_PARAMS
		// end add //
		
		int nsmps = sc_min(grain->counter, inNumSamples);
		if (grain->interp >= 4) {
			for (int j=0; j<nsmps; ++j) {
				GRAIN_BUF_LOOP_BODY_4
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		} else if (grain->interp >= 2) {
			for (int j=0; j<nsmps; ++j) {
				GRAIN_BUF_LOOP_BODY_2
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		} else {
			for (int j=0; j<nsmps; ++j) {
				GRAIN_BUF_LOOP_BODY_1
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		}
		
		grain->phase = phase;
		
		SAVE_GRAIN_AMP_PARAMS
		
		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[--unit->mNumActive];
		} else ++i;
	    }
	
		if ((trig > 0) && (unit->curtrig <=0)) {
			// start a grain
			if (unit->mNumActive+1 >= kMaxSynthGrains) {Print("Too many grains!\n");
			} else {
			float winType = IN0(7);
			if(winType >= 0.) { 
			    GET_GRAIN_WIN
			    }
			if((windowData) || (winType < 0.)) {
				uint32 bufnum = (uint32)IN0(2);
				
				GRAIN_BUF
				
				float bufSampleRate = buf->samplerate;
				float bufRateScale = bufSampleRate * SAMPLEDUR;
				double loopMax = (double)bufFrames;

				GrainBufG *grain = unit->mGrains + unit->mNumActive++;
				grain->bufnum = bufnum;
				
				double counter = IN0(1) * SAMPLERATE;
				counter = sc_max(4., counter);
				grain->counter = (int)counter;
							
				double rate = grain->rate = IN0(3) * bufRateScale;
				double phase = IN0(4) * bufFrames;
				grain->interp = (int)IN0(5);
				grain->winType = winType;
	    
				GET_GRAIN_INIT_AMP
				
				// begin add //
				float pan = IN0(6);
				
				CALC_GRAIN_PAN

				WRAP_CHAN_K
				    
				// end add //			
				int nsmps = sc_min(grain->counter, inNumSamples);
				if (grain->interp >= 4) {
					for (int j=0; j<nsmps; ++j) {
						GRAIN_BUF_LOOP_BODY_4
						CALC_NEXT_GRAIN_AMP
						phase += rate;
					}
				} else if (grain->interp >= 2) {
					for (int j=0; j<nsmps; ++j) {
						GRAIN_BUF_LOOP_BODY_2
						CALC_NEXT_GRAIN_AMP
						phase += rate;
					}
				} else {
					for (int j=0; j<nsmps; ++j) {
						GRAIN_BUF_LOOP_BODY_1
						CALC_NEXT_GRAIN_AMP
						phase += rate;
					}
				}
				
				grain->phase = phase;
				
				SAVE_GRAIN_AMP_PARAMS
				
				if (grain->counter <= 0) {
					// remove grain
					*grain = unit->mGrains[--unit->mNumActive];
				}
			}
		    }
	    }
	    unit->curtrig = trig;
	
}

void GrainBuf_Ctor(GrainBuf *unit)
{	
	if (INRATE(0) == calc_FullRate) 
	    SETCALC(GrainBuf_next_a);
	    else
	    SETCALC(GrainBuf_next_k);	
	unit->mNumActive = 0;
	unit->curtrig = 0.f;	
	GrainBuf_next_k(unit, 1); // should be _k
}

#define BUF_GRAIN_LOOP_BODY_4_N \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase * bufChannels; \
		float* table0 = table1 - bufChannels; \
		float* table2 = table1 + bufChannels; \
		float* table3 = table2 + bufChannels; \
		if (iphase == 0) { \
			table0 += bufSamples; \
		} else if (iphase >= guardFrame) { \
			if (iphase == guardFrame) { \
				table3 -= bufSamples; \
			} else { \
				table2 -= bufSamples; \
				table3 -= bufSamples; \
			} \
		} \
		float fracphase = phase - (double)iphase; \
		float a = table0[n]; \
		float b = table1[n]; \
		float c = table2[n]; \
		float d = table3[n]; \
		float outval = amp * cubicinterp(fracphase, a, b, c, d); \
		ZXP(out1) += outval; \

#define BUF_GRAIN_LOOP_BODY_2_N \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float* table1 = bufData + iphase * bufChannels; \
		float* table2 = table1 + bufChannels; \
		if (iphase > guardFrame) { \
			table2 -= bufSamples; \
		} \
		float fracphase = phase - (double)iphase; \
		float b = table1[n]; \
		float c = table2[n]; \
		float outval = amp * (b + fracphase * (c - b)); \
		ZXP(out1) += outval; \
		
// amp needs to be calculated by looking up values in window

#define BUF_GRAIN_LOOP_BODY_1_N \
		phase = sc_gloop(phase, loopMax); \
		int32 iphase = (int32)phase; \
		float outval = amp * bufData[iphase + n]; \
		ZXP(out1) += outval; \

void Warp1_next(Warp1 *unit, int inNumSamples)
{
	ClearUnitOutputs(unit, inNumSamples);
	
	GET_BUF
	SETUP_OUT
	CHECK_BUF	
	
	World *world = unit->mWorld;
	uint32 numBufs = world->mNumSndBufs;
	double amp = 0.;
	double winPos, winInc, w, b1, y1, y2, y0;
	winPos = winInc = w = b1 = y1 = y2 = y0 = 0.;
	SndBuf *window;
	float *windowData __attribute__((__unused__));
	uint32 windowSamples __attribute__((__unused__)) = 0;
	uint32 windowFrames __attribute__((__unused__)) = 0;
	int windowGuardFrame = 0;
			
	for (uint32 n=0; n < numOutputs; n++){
	int nextGrain = unit->mNextGrain[n];
	for (int i=0; i < unit->mNumActive[n]; ) {
		WarpWinGrain *grain = unit->mGrains[n] + i;
		
		double loopMax = (double)bufFrames;

		double rate = grain->rate;
		double phase = grain->phase;
		GET_GRAIN_AMP_PARAMS
		float *out1 = out[n];
		int nsmps = sc_min(grain->counter, inNumSamples);
		if (grain->interp >= 4) {
			for (int j=0; j<nsmps; ++j) {
				BUF_GRAIN_LOOP_BODY_4_N
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		} else if (grain->interp >= 2) {
			for (int j=0; j<nsmps; ++j) {
				BUF_GRAIN_LOOP_BODY_2_N
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		} else {
			for (int j=0; j<nsmps; ++j) {
				BUF_GRAIN_LOOP_BODY_1_N
				CALC_NEXT_GRAIN_AMP
				phase += rate;
			}
		}
		
		grain->phase = phase;
		SAVE_GRAIN_AMP_PARAMS
		if (grain->counter <= 0) {
			// remove grain
			*grain = unit->mGrains[n][--unit->mNumActive[n]];
		} else ++i;
	}
	
	for (int i=0; i<inNumSamples; ++i) {
		--nextGrain;
		if (nextGrain == 0) {
			// start a grain
			if (unit->mNumActive[n]+1 >= kMaxGrains) break;
			uint32 bufnum = (uint32)IN_AT(unit, 0, i);
			if (bufnum >= numBufs) break;

			float bufSampleRate = buf->samplerate;
			float bufRateScale = bufSampleRate * SAMPLEDUR;
			double loopMax = (double)bufFrames;

			WarpWinGrain *grain = unit->mGrains[n] + unit->mNumActive[n]++;
			grain->bufnum = bufnum;
			
			RGET

			float overlaps = IN_AT(unit, 5, i);
			float counter = IN_AT(unit, 3, i) * SAMPLERATE;
			double winrandamt = frand2(s1, s2, s3) * (double)IN_AT(unit, 6, i);
			counter = sc_max(4., floor(counter + (counter * winrandamt)));
			grain->counter = (int)counter;
			
			nextGrain = (int)(counter / overlaps);
			
			unit->mNextGrain[n] = nextGrain;
			
			float rate = grain->rate = IN_AT(unit, 2, i) * bufRateScale;
			float phase = IN_AT(unit, 1, i) * (float)bufFrames;
			grain->interp = (int)IN_AT(unit, 7, i);
			float winType = grain->winType = (int)IN_AT(unit, 4, i); // the buffer that holds the grain shape
			GET_GRAIN_WIN
			if((windowData) || (winType < 0.)) {
			    GET_GRAIN_INIT_AMP
			    
			    float *out1 = out[n] + i;
			    
			    int nsmps = sc_min(grain->counter, inNumSamples - i);
			    if (grain->interp >= 4) {
				    for (int j=0; j<nsmps; ++j) {
					    BUF_GRAIN_LOOP_BODY_4_N
					    CALC_NEXT_GRAIN_AMP
					    phase += rate;
				    }
			    } else if (grain->interp >= 2) {
				    for (int j=0; j<nsmps; ++j) {
					    BUF_GRAIN_LOOP_BODY_2_N
					    CALC_NEXT_GRAIN_AMP
					    phase += rate;
				}
			    } else {
				    for (int j=0; j<nsmps; ++j) {
					    BUF_GRAIN_LOOP_BODY_1_N
					    CALC_NEXT_GRAIN_AMP
					    phase += rate;
				    }
			    }
			    
			    grain->phase = phase;
			    SAVE_GRAIN_AMP_PARAMS
			    // store random values
			    RPUT
			    // end change
			    if (grain->counter <= 0) {
				    // remove grain
				    *grain = unit->mGrains[n][--unit->mNumActive[n]];
			    }
		    }
		}
	}	
	
	unit->mNextGrain[n] = nextGrain;
	}
}

void Warp1_Ctor(Warp1 *unit)
{	
	SETCALC(Warp1_next);
		
	for(int i = 0; i < 16; i++){
	    unit->mNumActive[i] = 0;
	    unit->mNextGrain[i] = 1;
	    }
	    
	ClearUnitOutputs(unit, 1);
	unit->m_fbufnum = -1e9f;

}


////////////////////////////////////////////////////////////////////////////////////////////////////////

void load(InterfaceTable *inTable)
{
	ft = inTable;

	DefineSimpleCantAliasUnit(GrainIn);
	DefineSimpleCantAliasUnit(GrainSin);
	DefineSimpleCantAliasUnit(GrainFM);	
	DefineSimpleCantAliasUnit(GrainBuf);
	DefineSimpleCantAliasUnit(Warp1);

}
