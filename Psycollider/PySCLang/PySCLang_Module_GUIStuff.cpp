/*
 * File: PYSCLang_Module_GUIStuff.cpp
 * Project : Psycollider
 * 
 * by:
 * Benjamin Golinvaux
 * benjamin.golinvaux@euresys.com
 * messenger: bgolinvaux@hotmail.com
 * 
 * currently maintained by:
 * Christopher Frauenberger 
 * frauenberger@iem.at
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 *
 */

#ifndef SC_WIN32
#include "pycxx/cxx/Objects.hxx"
#include "pycxx/cxx/Extensions.hxx"
#else
#include "stdafx.h"
#endif

#include "PyrPrimitive.h"
#include "PyrObject.h"
#include "PyrKernel.h"
#include "VMGlobals.h"
#include "SC_RGen.h"
#include "GC.h"
#include "PySCLang_Module.h"

int prOpenWinTextFile(struct VMGlobals *g, int numArgsPushed);
int prOpenWinTextFile(struct VMGlobals *g, int numArgsPushed)
{

  if (!g->canCallOS) 
    return errCantCallOS;

  PyrSlot *d = g->sp - 3;
	PyrSlot *a = g->sp - 2;
	PyrSlot *b = g->sp - 1;
	PyrSlot *c = g->sp; // tagObj for 
	
  int rangeStart, rangeSize;

  // retrieve path
  if (!(isKindOfSlot(a, class_string))) 
    return errWrongType;
	PyrString* string = a->uos;
  if(string->size == 0) 
    return errFailed;


  // start char in sel range
  int err = slotIntVal(b, &rangeStart);
  if (err) 
    return err;
  // sel range size
  err = slotIntVal(c, &rangeSize);
  if (err) 
    return err;

  if( PySCLang_Module::PyPrOpenWinTextFile_s != NULL) {
    // TODO : send text in the scLogSink_
    char* szString = new char[string->size+1];
    memcpy(szString,string->s,string->size);
    szString[string->size] = 0;
   PyObject* pystr = PyString_FromString(szString);
    delete [] szString;
    PyObject* pyRangeStart = PyInt_FromLong(rangeStart);
    PyObject* pyRangeSize = PyInt_FromLong(rangeSize);
    PyObject* tuple = PyTuple_New(3);
    PyTuple_SetItem(tuple,0,pystr);
    PyTuple_SetItem(tuple,1,pyRangeStart);
    PyTuple_SetItem(tuple,2,pyRangeSize);

   /* make the Python call thread safe (global interpreter clock) */
	PyGILState_STATE gstate;	
	gstate = PyGILState_Ensure();
    PyObject* result = PyObject_Call(PySCLang_Module::PyPrOpenWinTextFile_s,tuple,NULL);
	PyGILState_Release(gstate);

	result = PyErr_Occurred( );
	if (!result) {
		post("logSink call failed, result: %d\n", result);
		return errFailed;
	}
  }
  else { // no log sink callable..
    post("No Python callable bound to PySCLang_Module::PyPrOpenWinTextFile_s with setPyPrOpenWinTextFile\n");
    return errFailed;
  }
  //NSString *nsstring = [NSString stringWithCString: string->s length: string->size];
        
  //SetPtr(d->uo->slots + 0, doc); // what should we do with this
	return errNone;
}

void initGUIPrimitives()
{
  int base,index;

	base = nextPrimitiveIndex();
	index = 0;
	
	definePrimitive(base, index++, "_OpenWinTextFile", prOpenWinTextFile, 3, 0);	
}



