/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 c-style: "K&R" -*- */
/* 
   jep - Java Embedded Python

   Copyright (c) 2004 - 2011 AUTHORS.

   This file is licenced under the the zlib/libpng License.

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.
   
   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
   must not claim that you wrote the original software. If you use
   this software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
   must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.   
*/ 	

#ifdef WIN32
# include "winconfig.h"
#endif

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#include "Python.h"
#include <jni.h>

#include "pyjboolean.h"
#include "pyjtype.h"
#include "pyjconverter.h"

PyObject* pyjboolean_jtopy(JNIEnv* env, jvalue jval){
    return PyInt_FromLong(jval.i);
}

jvalue pyjboolean_from_pyboolean(JNIEnv* env, PyObject* pyobj){
    jvalue value;
    value.z = PyInt_AsLong(pyobj);
    return value;
}

jvalue pyjbooleanobj_from_pyboolean(JNIEnv* env, PyObject* pyobj){
    jclass booleanobj = (*env)->FindClass(env, "java/lang/Boolean");
    jmethodID method = (*env)->GetMethodID(env, booleanobj, "<init>", "(Z)V");
    jvalue value;
    value.z = PyInt_AsLong(pyobj);
    value.l = (*env)->NewObject(env, booleanobj, method, value);
    return value;
}

int pyjboolean_extend(PyJTypeObject* type){
    type->pyjtp_jtopy = pyjboolean_jtopy;
    return 0;
}


void PyJBoolean_Init(void){
    PyJType_Extend("boolean", pyjboolean_extend);

    PyJConverter_Register("boolean", 
                          (PyObject*) &PyInt_Type, 
                           0, 
                           pyjboolean_from_pyboolean);
    PyJConverter_Register("Boolean", 
                          (PyObject*) &PyInt_Type, 
                          1, 
                          pyjbooleanobj_from_pyboolean);
}

PyObject* PyJBoolean_jtopy(JNIEnv* env, jboolean z){
	PyJTypeObject* pyjtype = NULL;
    jvalue value           = {.z = z };
    pyjtype = (PyJTypeObject*) PyJType_PrimitiveFromName(env, "boolean");
    if(pyjtype == NULL){
    	return NULL;
    }
    return pyjtype->pyjtp_jtopy(env, value);
}
