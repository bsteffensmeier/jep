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

#include "pyjstring.h"
#include "pyjtype.h"
#include "pyjconverter.h"

PyObject* pyjstring_jtopy(JNIEnv* env, jvalue jval){
    PyObject* pyobj;
    const char* s;
    s = (*env)->GetStringUTFChars(env, jval.l, 0);
    pyobj = PyString_FromString(s);
    (*env)->ReleaseStringUTFChars(env, jval.l, s);
    return pyobj;
}

jvalue pyjstring_from_pystring(JNIEnv* env, PyObject* pyobj){
    jvalue value;
    const char* s = PyString_AsString(pyobj);
    value.l = (*env)->NewStringUTF(env, s);
    return value;
}

int pyjstring_extend(PyJTypeObject* type){
    type->pyjtp_jtopy = pyjstring_jtopy;
    return 0;
}


void PyJString_Init(void){
    PyJType_Extend("java.lang.String", pyjstring_extend);

    PyJConverter_Register("java.lang.String", 
                          (PyObject*) &PyString_Type, 
                           0, 
                           pyjstring_from_pystring);
}
