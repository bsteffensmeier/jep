/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 c-style: "K&R" -*- */
/* 
   jep - Java Embedded Python

   Copyright (c) 2004 - 2015 AUTHORS.

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

#ifndef _Included_pyjobject2
#define _Included_pyjobject2

/*
 * This can be used for the tp_str method of any PyJTypeObject that properly 
 * defines pyjtp_pytoj.
 */
PyAPI_FUNC(PyObject*) PyJObject2_GenericStr(PyObject*);

/*
 * This can be used for the tp_hash method of any PyJTypeObject that properly 
 * defines pyjtp_pytoj.
 */
PyAPI_FUNC(long) PyJObject2_GenericHash(PyObject*);

/*
 * This can be used for the tp_richcompare method of any PyJTypeObject that 
 * properly defines pyjtp_pytoj.
 */
PyAPI_FUNC(PyObject*) PyJObject2_GenericCompare(PyObject*, PyObject*, int);

/*
 * This can be used for the tp_alloc method of any PyJTypeObject.
 */
PyAPI_FUNC(PyObject*) PyJObject2_GenericAlloc(PyTypeObject*, Py_ssize_t);

/*
 * Convert a java object to a python object of the appropriate type.
 */
PyAPI_FUNC(PyObject*) PyJObject2_jtopy(JNIEnv*, jobject);

typedef struct {
    PyObject_HEAD
    jobject object;
} PyJObject2Object;

/*
 * Called by PyJType_Init, not by anyone else.
 */
int _PyJObject2_Init(JNIEnv*);

/* Everyone always seems to need access to java.lang.Object so here it is. */
PyAPI_DATA(jclass) jclass_java_lang_Object;

PyAPI_DATA(jmethodID) jmethod_object_toString;

#endif // ndef pyjobject2
