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

#include "pyjfloat.h"
#include "pyjtype.h"
#include "pyjconverter.h"

PyObject* pyjfloat_jtopy(JNIEnv* env, jvalue jval){
    return PyFloat_FromDouble(jval.i);
}

jvalue pyjfloat_from_pyfloat(JNIEnv* env, PyObject* pyobj){
    jvalue value;
    value.f = PyFloat_AsDouble(pyobj);
    return value;
}

jvalue pyjfloatobj_from_pyfloat(JNIEnv* env, PyObject* pyobj){
    jclass floatobj = (*env)->FindClass(env, "java/lang/Float");
    jmethodID method = (*env)->GetMethodID(env, floatobj, "<init>", "(F)V");
    jvalue value;
    value.f = PyFloat_AsDouble(pyobj);
    value.l = (*env)->NewObject(env, floatobj, method, value);
    return value;
}

int pyjfloat_extend(PyJTypeObject* type){
    type->pyjtp_jtopy = pyjfloat_jtopy;
    return 0;
}


void PyJFloat_Init(void){
    PyJType_Extend("float", pyjfloat_extend);

    PyJConverter_Register("float", 
                          (PyObject*) &PyFloat_Type, 
                           0, 
                           pyjfloat_from_pyfloat);
    PyJConverter_Register("Float", 
                          (PyObject*) &PyFloat_Type, 
                          1, 
                          pyjfloatobj_from_pyfloat);
}

PyObject* PyJFloat_jtopy(JNIEnv* env, jfloat f){
	PyJTypeObject* pyjtype = NULL;
    jvalue value           = {.f = f };
    pyjtype = (PyJTypeObject*) PyJType_PrimitiveFromName(env, "float");
    if(pyjtype == NULL){
    	return NULL;
    }
    return pyjtype->pyjtp_jtopy(env, value);
}
