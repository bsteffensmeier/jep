/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 c-style: "K&R" -*- */
/*
   jep - Java Embedded Python

   Copyright (c) 2016 JEP AUTHORS.

   This file is licensed under the the zlib/libpng License.

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

/*
 * A PyJClassObject is a PyJObject with a __call__ method attached, where
 * the call method can invoke the Java object's constructors.
 */

#ifndef _Included_pyjtype
#define _Included_pyjtype

PyAPI_FUNC(int) PyJType_Check(PyObject*);
PyAPI_FUNC(PyTypeObject*) PyJType_Wrap(JNIEnv* env, jclass clazz);

PyAPI_FUNC(jclass) PyJType_GetClass(PyObject*);

PyAPI_DATA(PyTypeObject) PyJType_Type;

#endif // ndef pyjtype
