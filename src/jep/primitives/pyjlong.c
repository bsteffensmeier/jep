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

#include "pyjlong.h"
#include "pyjtype.h"
#include "pyjconverter.h"

PyObject* pyjlong_jtopy(JNIEnv* env, jvalue jval){
    return PyLong_FromLong(jval.i);
}

jvalue pyjlong_from_pylong(JNIEnv* env, PyObject* pyobj){
    jvalue value;
    value.j = PyLong_AsLong(pyobj);
    return value;
}

jvalue pyjlongobj_from_pylong(JNIEnv* env, PyObject* pyobj){
    jclass longobj = (*env)->FindClass(env, "java/lang/Long");
    jmethodID method = (*env)->GetMethodID(env, longobj, "<init>", "(L)V");
    jvalue value;
    value.j = PyLong_AsLong(pyobj);
    value.l = (*env)->NewObject(env, longobj, method, value);
    return value;
}

int pyjlong_extend(PyJTypeObject* type){
    type->pyjtp_jtopy = pyjlong_jtopy;
    return 0;
}


void PyJLong_Init(void){
    PyJType_Extend("long", pyjlong_extend);

    PyJConverter_Register("long", 
                          (PyObject*) &PyLong_Type, 
                           0, 
                           pyjlong_from_pylong);
    PyJConverter_Register("Long", 
                          (PyObject*) &PyLong_Type, 
                          1, 
                          pyjlongobj_from_pylong);
}

PyObject* PyJLong_jtopy(JNIEnv* env, jlong j){
	PyJTypeObject* pyjtype = NULL;
    jvalue value           = {.j = j };
    pyjtype = (PyJTypeObject*) PyJType_PrimitiveFromName(env, "long");
    if(pyjtype == NULL){
    	return NULL;
    }
    return pyjtype->pyjtp_jtopy(env, value);
}
