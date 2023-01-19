/*
   jep - Java Embedded Python

   Copyright (c) 2023 JEP AUTHORS.

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

   Defines the python type for the java int primitive array.
   This does not define any code, just sets the correct macros
   for the pyjprimitivearray template.
*/

#define PyJPrimitiveArray_InitType PyJIntArray_InitType

#define ARRAY_TP_NAME "[I"
#define ARRAY_TP_DOC "Jep java int array"

#define jprimitive jint
#define jprimitiveArray jintArray

#define JPRIMITIVE_ARRAY_TYPE JINT_ARRAY_TYPE

#define jprimitive_As_PyObject jint_As_PyObject
#define PyObject_As_jprimitive PyObject_As_jint

#define NewPrimitiveArray NewIntArray
#define GetPrimitiveArrayElements GetIntArrayElements
#define ReleasePrimitiveArrayElements ReleaseIntArrayElements
#define GetPrimitiveArrayRegion GetIntArrayRegion
#define SetPrimitiveArrayRegion SetIntArrayRegion

#include "pyjprimitivearray_template.c"
