/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 c-style: "K&R" -*- */
/* 
   jep - Java Embedded Python

   Copyright (c) 2004 - 2011 Mike Johnson.

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


// shut up the compiler
#ifdef _POSIX_C_SOURCE
#  undef _POSIX_C_SOURCE
#endif
#include <jni.h>
#include <Python.h>

#ifndef _Included_pyjmethod2
#define _Included_pyjmethod2

#include "pyjtype.h"

/*
 * Called by PyJType_Init, not by anyone else.
 */
int _PyJMethod2_Init(JNIEnv*);

typedef struct jReflectMethodListNode_ {
  jobject reflect_method;
  struct jReflectMethodListNode_* next;
} jReflectMethodListNode;

typedef struct jmethodListNode_ {
  jmethodID method;
  PyJTypeObject* returnType;
  PyJTypeObject** parameterTypes;
  struct jmethodListNode_* next;
} jmethodListNode;

typedef struct {
    PyObject_HEAD
    int initialized;
    PyObject* name;
    jReflectMethodListNode* relfectMethodList;
    jmethodListNode* methodList[10];
} PyJMethod2_Object;

PyAPI_DATA(PyJTypeObject) PyJMethod2_Type;
PyAPI_FUNC(int) pyjmethod2_init_jni_globals(JNIEnv*);
PyAPI_FUNC(int) PyJMethod2_Get_Methods(JNIEnv*, jclass, PyObject*);

#endif // ndef pyjmethod2
