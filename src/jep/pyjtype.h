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

#ifndef _Included_pyjtype
#define _Included_pyjtype

/*
 * Enable use of PyJType. This must be done first thing when jep is loaded,
 * after the interpreter is enabled so that java types can be loaded.
 */
PyAPI_FUNC(int) PyJType_Init(JNIEnv*);

/*
 * Make a jclass into a PyObject*. This should be equivelant to
 * PyJType_Type.pyjtp_jtopy(env, {.l = class}).
 */
PyAPI_FUNC(PyObject*) PyJType_jtopy(JNIEnv*, jclass);

/*
 * Load a class with the given name and wrap it up in PyJTypeObject.
 * Classname should be seperate by '.', not the jni cnvention of seperation by '/'.
 * TODO identify how this works with arrays.
 */
PyAPI_FUNC(PyObject*) PyJType_FromName(JNIEnv*, const char*);

/*
 * Load a class with the given name and wrap it up in PyJTypeObject.
 * Since primitives cannot normally be loaded by name this method must be used
 * instead of PyJType_FromName
 */
PyAPI_FUNC(PyObject*) PyJType_PrimitiveFromName(JNIEnv*, const char*);

/*
 * A jtopy method is called whenever a java object is used in python. In
 * general most objects should just wrap the jobject, however some types
 * may instead choose to convert the object to its python equivalent. It
 * should be possible to pass the result of jtopy back into python. For
 * wrapper types this is done by defining a pyjtp_pytoj method on the type.
 * For types that convert it should be done py registering a pytoj method
 * with the PyJValueConverter.
 */
typedef PyObject* (*jtopy)(JNIEnv*, jvalue);

/*
 * Undo jtopy. For most type it should only be necessary to override this if the
 * pyjtp_pytype defines a custom object.
 */
typedef jvalue (*pytoj)(JNIEnv*, PyObject*);

/*
 * A object which wraps a java Class and defines a python type for that class.
 * The first member is a PyTypeObject so it is safe to cast a PyjTypeObject* to
 * a PyTypeObject*.
 */
typedef struct {
  PyTypeObject pyjtp_pytype;
  jclass pyjtp_class;
  jtopy pyjtp_jtopy;
  pytoj pyjtp_pytoj;
} PyJTypeObject;

/* The type of all PyJTypObjects */
PyAPI_DATA(PyJTypeObject) PyJType_Type;

/*
 * Determine if a python object is an instance of a type which is an instance of
 * PyJtype. More simply return the PyJType of the object if it is a wrapper around
 * a java object. If the object is not a wrapper around a java object this function
 * return NULL. This method does not set an error.
 */
PyAPI_FUNC(PyJTypeObject*) PyJType_GetPyJType(PyObject*);

/*
 * Method used to extend a PyJType with custom pythonic behavior. Methods
 * should return zero on success and nonzero on failure, setting an appropriate
 * PyErr. Failure will result in the class failing to load.
 */
typedef int (*pyjtype_extend)(PyJTypeObject*);

/* A PyJType is not loaded until it is needed. Extenders can be registered
 * during jep initialization so that when the type is loaded custom behavior
 * can be added. For example any java.lang.Iterable objects should implement
 * the python iterable methods.
 */
PyAPI_FUNC(void) PyJType_Extend(const char *, pyjtype_extend);


/*
 * Everyone always seems to need access to java.lang.Class so here it is.
 */
PyAPI_DATA(jclass) jclass_java_lang_Class;

#endif // ndef pyjtype
