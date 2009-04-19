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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/


#ifndef _SC_WorldOptions_
#define _SC_WorldOptions_

#include <stdarg.h>
#include "SC_Types.h"

#ifdef SC_DARWIN
#include <CoreFoundation/CFString.h>
#endif

typedef int (*PrintFunc)(const char *format, va_list ap);

struct WorldOptions
{
	const char* mPassword;
	uint32 mNumBuffers;
	uint32 mMaxLogins;
	uint32 mMaxNodes;
	uint32 mMaxGraphDefs;
	uint32 mMaxWireBufs;
	uint32 mNumAudioBusChannels;
	uint32 mNumInputBusChannels;
	uint32 mNumOutputBusChannels;
	uint32 mNumControlBusChannels;
	uint32 mBufLength;
	uint32 mRealTimeMemorySize;

	int mNumSharedControls;
	float *mSharedControls;
	
	bool mRealTime;
	bool mMemoryLocking;
	
	const char *mNonRealTimeCmdFilename;
	const char *mNonRealTimeInputFilename;
	const char *mNonRealTimeOutputFilename;
	const char *mNonRealTimeOutputHeaderFormat;
	const char *mNonRealTimeOutputSampleFormat;
	
	uint32 mPreferredSampleRate;
	uint32 mNumRGens;
	
	uint32 mPreferredHardwareBufferFrameSize;
	
	uint32 mLoadGraphDefs;
	
#ifdef SC_DARWIN
	const char *mInputStreamsEnabled;
	const char *mOutputStreamsEnabled;
    CFStringRef mServerPortName;
    CFStringRef mReplyPortName;
#endif
	const char *mInDeviceName;
	
	int mVerbosity;
	
	bool mRendezvous;
	
	const char *mUGensPluginPath;
	
	const char *mOutDeviceName;
	
	const char *mRestrictedPath;
};

const WorldOptions kDefaultWorldOptions = 
{
	0,1024,64,1024,1024,64,128,8,8,4096,64,8192, 0,0, 1, 0, 0,0,0,0,0,
#ifdef SC_WIN32
	44100, 
#else
	0,
#endif
	64, 0, 1
#ifdef SC_DARWIN
	,0,0,CFSTR("com.audiosynth.scsynth"),NULL
#endif
	,0
	,0
	,1
	,0
	,0
	,0
};

//#ifdef SC_WIN32
//#include "../../Headers/server/SC_Reply.h"
//#else //SC_WIN32
#include "SC_Reply.h"
//#endif //SC_WIN32

#ifdef SC_WIN32
# define SC_DLLEXPORT __declspec(dllexport)
#else
# define SC_DLLEXPORT
#endif

extern "C" {
	SC_DLLEXPORT void SetPrintFunc(PrintFunc func);
	SC_DLLEXPORT struct World* World_New(WorldOptions *inOptions);
	SC_DLLEXPORT void World_Cleanup(World *inWorld);
	SC_DLLEXPORT void World_NonRealTimeSynthesis(struct World *inWorld, WorldOptions *inOptions);
	SC_DLLEXPORT int World_OpenUDP(struct World *inWorld, int inPort);
	SC_DLLEXPORT int World_OpenTCP(struct World *inWorld, int inPort, int inMaxConnections, int inBacklog);
#ifdef SC_DARWIN
	SC_DLLEXPORT void World_OpenMachPorts(struct World *inWorld, CFStringRef localName, CFStringRef remoteName);
#endif
	SC_DLLEXPORT void World_WaitForQuit(struct World *inWorld);
	SC_DLLEXPORT bool World_SendPacket(struct World *inWorld, int inSize, char *inData, ReplyFunc inFunc);
	SC_DLLEXPORT bool World_SendPacketWithContext(struct World *inWorld, int inSize, char *inData, ReplyFunc inFunc, void *inContext);
	SC_DLLEXPORT int World_CopySndBuf(World *world, uint32 index, struct SndBuf *outBuf, bool onlyIfChanged, bool &didChange);
	SC_DLLEXPORT int scprintf(const char *fmt, ...);
}

#endif // _SC_WorldOptions_
