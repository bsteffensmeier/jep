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

#include "pyjint.h"
#include "pyjtype.h"
#include "pyjconverter.h"

PyObject* pyjint_jtopy(JNIEnv* env, jvalue jval){
    return PyInt_FromLong(jval.i);
}

jvalue pyjint_from_pyint(JNIEnv* env, PyObject* pyobj){
    jvalue value;
    value.i = PyInt_AsLong(pyobj);
    return value;
}

jvalue pyjinteger_from_pyint(JNIEnv* env, PyObject* pyobj){
    jclass integer = (*env)->FindClass(env, "java/lang/Integer");
    jmethodID method = (*env)->GetMethodID(env, integer, "<init>", "(I)V");
    jvalue value;
    value.i = PyInt_AsLong(pyobj);
    value.l = (*env)->NewObject(env, integer, method, value);
    return value;
}

int pyjint_extend(PyJTypeObject* type){
    type->pyjtp_jtopy = pyjint_jtopy;
    return 0;
}


void PyJInt_Init(void){
    PyJType_Extend("int", pyjint_extend);

    PyJConverter_Register("int", 
                          (PyObject*) &PyInt_Type, 
                           0, 
                           pyjint_from_pyint);
    PyJConverter_Register("Integer", 
                          (PyObject*) &PyInt_Type, 
                          1, 
                          pyjinteger_from_pyint);
}

PyObject* PyJInt_jtopy(JNIEnv* env, jint i){
	PyJTypeObject* pyjtype = NULL;
    jvalue value           = {.i = i };
    pyjtype = (PyJTypeObject*) PyJType_PrimitiveFromName(env, "int");
    if(pyjtype == NULL){
    	return NULL;
    }
    return pyjtype->pyjtp_jtopy(env, value);
}
