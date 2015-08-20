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

#include "pyjdouble.h"
#include "pyjtype.h"
#include "pyjconverter.h"

PyObject* pyjdouble_jtopy(JNIEnv* env, jvalue jval){
    return PyFloat_FromDouble(jval.i);
}

jvalue pyjdouble_from_pydouble(JNIEnv* env, PyObject* pyobj){
    jvalue value;
    value.d = PyFloat_AsDouble(pyobj);
    return value;
}

jvalue pyjdoubleobj_from_pydouble(JNIEnv* env, PyObject* pyobj){
    jclass doubleobj = (*env)->FindClass(env, "java/lang/Double");
    jmethodID method = (*env)->GetMethodID(env, doubleobj, "<init>", "(D)V");
    jvalue value;
    value.d = PyFloat_AsDouble(pyobj);
    value.l = (*env)->NewObject(env, doubleobj, method, value);
    return value;
}

int pyjdouble_extend(PyJTypeObject* type){
    type->pyjtp_jtopy = pyjdouble_jtopy;
    return 0;
}


void PyJDouble_Init(void){
    PyJType_Extend("double", pyjdouble_extend);

    PyJConverter_Register("double", 
                          (PyObject*) &PyFloat_Type, 
                           0, 
                           pyjdouble_from_pydouble);
    PyJConverter_Register("Double", 
                          (PyObject*) &PyFloat_Type, 
                          1, 
                          pyjdoubleobj_from_pydouble);
}

PyObject* PyJDouble_jtopy(JNIEnv* env, jdouble d){
	PyJTypeObject* pyjtype = NULL;
    jvalue value           = {.d = d };
    pyjtype = (PyJTypeObject*) PyJType_PrimitiveFromName(env, "double");
    if(pyjtype == NULL){
    	return NULL;
    }
    return pyjtype->pyjtp_jtopy(env, value);
}
