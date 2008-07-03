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



#include "SC_Lib_Cintf.h"
#include "SC_ComPort.h"
#include "SC_CoreAudio.h"
#include "SC_UnitDef.h"
#include "SC_BufGen.h"
#include "SC_World.h"
#include "SC_StringParser.h"
#include "SC_InterfaceTable.h"
#include "SC_DirUtils.h"
#include <stdexcept>
#ifndef _MSC_VER
#include <dirent.h>
#endif //_MSC_VER

#ifndef SC_WIN32
#include <dlfcn.h>
#endif

#ifdef SC_WIN32
#include "SC_Win32Utils.h"
#else
#include <libgen.h>
#endif

// Plugin directory in resource directory
#if defined(SC_WIN32) && defined(_DEBUG)
# define SC_PLUGIN_DIR_NAME "plugins_debug"
#else
# define SC_PLUGIN_DIR_NAME "plugins"
#endif

// Extension for binary plugins
#ifndef SC_PLUGIN_EXT
# define SC_PLUGIN_EXT ".scx"
#endif

// Symbol of initialization routine when loading plugins
#ifndef SC_PLUGIN_LOAD_SYM

# ifdef SC_DARWIN
#  define SC_PLUGIN_LOAD_SYM "load"
# else
#  define SC_PLUGIN_LOAD_SYM "_load"
# endif

#endif

#ifndef SC_WIN32
# include <sys/param.h>
#endif

#ifdef SC_DARWIN
extern "C" {
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
}
char gTempVal;
#endif

Malloc gMalloc;
HashTable<SC_LibCmd, Malloc> *gCmdLib;
HashTable<struct UnitDef, Malloc> *gUnitDefLib = 0;
HashTable<struct BufGen, Malloc> *gBufGenLib = 0;
HashTable<struct PlugInCmd, Malloc> *gPlugInCmds = 0;
extern struct InterfaceTable gInterfaceTable;
SC_LibCmd* gCmdArray[NUMBER_OF_COMMANDS];

void initMiscCommands();
bool PlugIn_LoadDir(const char *dirname, bool reportError);

#ifdef SC_DARWIN
void read_section(const struct mach_header *mhp, unsigned long slide, const char *segname, const char *sectname)
{
	u_int32_t size;	
	char *sect = getsectdatafromheader(mhp, segname, sectname, &size);
	if(!sect) return;

	char *start = sect + slide;
	char *end = start + size;

	while(start != end) {
		gTempVal += *(char *)start;
		start++;
	}
}
#endif

void initialize_library()
{	
	gCmdLib     = new HashTable<SC_LibCmd, Malloc>(&gMalloc, 64, true);
	gUnitDefLib = new HashTable<UnitDef, Malloc>(&gMalloc, 512, true);
	gBufGenLib  = new HashTable<BufGen, Malloc>(&gMalloc, 512, true);
	gPlugInCmds = new HashTable<PlugInCmd, Malloc>(&gMalloc, 64, true);

	initMiscCommands();
	
#ifdef SC_PLUGIN_DIR
	// load globally installed plugins
	if (sc_DirectoryExists(SC_PLUGIN_DIR)) {
		PlugIn_LoadDir(SC_PLUGIN_DIR, true);
	}
#endif

	// load default plugin directory
	char pluginDir[MAXPATHLEN];
	sc_GetResourceDirectory(pluginDir, MAXPATHLEN);
	sc_AppendToPath(pluginDir, SC_PLUGIN_DIR_NAME);

	if (sc_DirectoryExists(pluginDir)) {
		PlugIn_LoadDir(pluginDir, true);
	}
	
	// get extension directories
	char extensionDir[MAXPATHLEN];
	if (!sc_IsStandAlone()) {
 	// load system extension plugins
	sc_GetSystemExtensionDirectory(extensionDir, MAXPATHLEN);
 	PlugIn_LoadDir(extensionDir, false);
 	
 	// load user extension plugins
	sc_GetUserExtensionDirectory(extensionDir, MAXPATHLEN);
 	PlugIn_LoadDir(extensionDir, false);

	// load user plugin directories
	SC_StringParser sp(getenv("SC_PLUGIN_PATH"), ':');
	while (!sp.AtEnd()) {
		PlugIn_LoadDir(const_cast<char *>(sp.NextToken()), true);
	}
	}
#ifdef SC_DARWIN
	/* on darwin plugins are lazily loaded (dlopen uses mmap internally), which can produce audible
		glitches when UGens have to be paged-in. to work around this we preload all the plugins by
		iterating through their memory space. */

	unsigned long images = _dyld_image_count();
	for(unsigned long i = 0; i < images; i++) {
		const mach_header	*hdr = _dyld_get_image_header(i);
		unsigned long slide = _dyld_get_image_vmaddr_slide(i);
		const char *name = _dyld_get_image_name(i);
		uint32_t	size;
		char *sect;

		if(!strcmp(name + (strlen(name) - 4), ".scx")) {
			read_section(hdr, slide, "__TEXT", "__text");
			read_section(hdr, slide, "__TEXT", "__const");
			read_section(hdr, slide, "__TEXT", "__cstring");
			read_section(hdr, slide, "__TEXT", "__picsymbol_stub");
			read_section(hdr, slide, "__TEXT", "__symbol_stub");
			read_section(hdr, slide, "__TEXT", "__const");
			read_section(hdr, slide, "__TEXT", "__literal4");
			read_section(hdr, slide, "__TEXT", "__literal8");

			read_section(hdr, slide, "__DATA", "__data");
			read_section(hdr, slide, "__DATA", "__la_symbol_ptr");
			read_section(hdr, slide, "__DATA", "__nl_symbol_ptr");
			read_section(hdr, slide, "__DATA", "__dyld");
			read_section(hdr, slide, "__DATA", "__const");
			read_section(hdr, slide, "__DATA", "__mod_init_func");															
			read_section(hdr, slide, "__DATA", "__bss");
			read_section(hdr, slide, "__DATA", "__common");

			read_section(hdr, slide, "__IMPORT", "__jump_table");
			read_section(hdr, slide, "__IMPORT", "__pointers");			
		}
	}
#endif
}

bool PlugIn_Load(const char *filename);
bool PlugIn_Load(const char *filename)
{
#ifdef SC_WIN32

    HINSTANCE hinstance = LoadLibrary( filename );
    if (!hinstance) {
        char *s;
        DWORD lastErr = GetLastError();
        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                0, lastErr , 0, (char*)&s, 1, 0 );
        scprintf("*** ERROR: LoadLibrary '%s' err '%s'\n", filename, s);
        LocalFree( s );     
		return false;
	}

    void *ptr = GetProcAddress( hinstance, SC_PLUGIN_LOAD_SYM );
    if (!ptr) {
        char *s;
        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                0, GetLastError(), 0, (char*)&s, 1, 0 );
        scprintf("*** ERROR: GetProcAddress %s err '%s'\n", SC_PLUGIN_LOAD_SYM, s);
        LocalFree( s );

		FreeLibrary(hinstance);
		return false;
	}	

    LoadPlugInFunc loadFunc = (LoadPlugInFunc)ptr;
	(*loadFunc)(&gInterfaceTable);		

    // FIXME: at the moment we never call FreeLibrary() on a loaded plugin
    
	return true;

#else

	void* handle = dlopen(filename, RTLD_NOW);
	
	if (!handle) {
		scprintf("*** ERROR: dlopen '%s' err '%s'\n", filename, dlerror());
		dlclose(handle);
		return false;
	}
	
	void *ptr;
	
	ptr = dlsym(handle, SC_PLUGIN_LOAD_SYM);
	if (!ptr) {
		scprintf("*** ERROR: dlsym %s err '%s'\n", SC_PLUGIN_LOAD_SYM, dlerror());
		dlclose(handle);
		return false;
	}		
	
	LoadPlugInFunc loadFunc = (LoadPlugInFunc)ptr;
	(*loadFunc)(&gInterfaceTable);		

	return true;
    
#endif
}

bool PlugIn_LoadDir(const char *dirname, bool reportError)
{
	bool success = true;

	SC_DirHandle *dir = sc_OpenDir(dirname);
	if (!dir) {
		if (reportError) {
			scprintf("*** ERROR: open directory failed '%s'\n", dirname); fflush(stdout);
		}
		return false;
	}
	
	for (;;) {
		char diritem[MAXPATHLEN];
		bool skipItem = true;
		bool validItem = sc_ReadDir(dir, dirname, diritem, skipItem);
		if (!validItem) break;
		if (skipItem) continue;
		
        if (sc_DirectoryExists(diritem)) {
			success = PlugIn_LoadDir(diritem, reportError);
        } else {
			int dnamelen = strlen(diritem);
			int extlen = strlen(SC_PLUGIN_EXT);
			char *extptr = diritem+dnamelen-extlen;
			if (strncmp(extptr, SC_PLUGIN_EXT, extlen) == 0) {
				success = PlugIn_Load(diritem);
			}
        }

		if (!success) continue;
	}

	sc_CloseDir(dir);
	return success;
}
