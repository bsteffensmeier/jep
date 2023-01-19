/*
   jep - Java Embedded Python

   Copyright (c) 2004-2023 JEP AUTHORS.

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

#include "Jep.h"

// called internally to make new PyJArrayObject instances
PyObject* pyjarray_new(JNIEnv *env, jobjectArray obj)
{
    return jobject_As_PyJObject(env, obj, NULL);
}


// called from module to create new arrays.
// args are variable, should accept:
// (size, typeid, [value]), (size, jobject),
//     (size, pyjarray), (list)
PyObject* pyjarray_new_v(PyObject *isnull, PyObject *args)
{
    jclass           componentClass = NULL;
    JNIEnv          *env       = NULL;
    jobjectArray     arrayObj  = NULL;
    long             typeId    = -1;
    long             size      = -1;

    // args
    PyObject *one, *two, *three;
    one = two = three = NULL;

    env = pyembed_get_env();

    if (!PyArg_UnpackTuple(args, "ref", 2, 3, &one, &two, &three)) {
        return NULL;
    }

    if (PyLong_Check(one)) {
        size = (long) PyLong_AsLongLong(one);

        if (PyLong_Check(two)) {
            typeId = (int) PyLong_AsLongLong(two);

            if (size < 0) {
                return PyErr_Format(PyExc_ValueError, "Invalid size %li", size);
            }

            // make a new primitive array
            switch (typeId) {
            case JSTRING_ID:
                arrayObj = (*env)->NewObjectArray(env,
                                                  (jsize) size,
                                                  JSTRING_TYPE,
                                                  NULL);
                break;

            case JINT_ID:
                arrayObj = (*env)->NewIntArray(env, (jsize) size);
                break;

            case JLONG_ID:
                arrayObj = (*env)->NewLongArray(env, (jsize) size);
                break;

            case JBOOLEAN_ID:
                arrayObj = (*env)->NewBooleanArray(env, (jsize) size);
                break;

            case JDOUBLE_ID:
                arrayObj = (*env)->NewDoubleArray(env, (jsize) size);
                break;

            case JSHORT_ID:
                arrayObj = (*env)->NewShortArray(env, (jsize) size);
                break;

            case JFLOAT_ID:
                arrayObj = (*env)->NewFloatArray(env, (jsize) size);
                break;

            case JBYTE_ID:
                arrayObj = (*env)->NewByteArray(env, (jsize) size);
                break;

            case JCHAR_ID:
                arrayObj = (*env)->NewCharArray(env, (jsize) size);
                break;
            } // switch

        } // if int(two)
        else if (PyJObject_Check(two)) {
            PyJObject *pyjob = (PyJObject *) two;

            componentClass = pyjob->clazz;
            if ((*env)->IsAssignableFrom(env, componentClass, JOBJECT_TYPE)) {
                arrayObj = (*env)->NewObjectArray(env,
                                                  (jsize) size,
                                                  componentClass,
                                                  NULL);
            } else if ((*env)->IsSameObject(env, componentClass, JBOOLEAN_TYPE)) {
                arrayObj = (*env)->NewBooleanArray(env, (jsize) size);
            } else if ((*env)->IsSameObject(env, componentClass, JBYTE_TYPE)) {
                arrayObj = (*env)->NewByteArray(env, (jsize) size);
            } else if ((*env)->IsSameObject(env, componentClass, JCHAR_TYPE)) {
                arrayObj = (*env)->NewCharArray(env, (jsize) size);
            } else if ((*env)->IsSameObject(env, componentClass, JSHORT_TYPE)) {
                arrayObj = (*env)->NewShortArray(env, (jsize) size);
            } else if ((*env)->IsSameObject(env, componentClass, JINT_TYPE)) {
                arrayObj = (*env)->NewIntArray(env, (jsize) size);
            } else if ((*env)->IsSameObject(env, componentClass, JLONG_TYPE)) {
                arrayObj = (*env)->NewLongArray(env, (jsize) size);
            } else if ((*env)->IsSameObject(env, componentClass, JFLOAT_TYPE)) {
                arrayObj = (*env)->NewFloatArray(env, (jsize) size);
            } else if ((*env)->IsSameObject(env, componentClass, JDOUBLE_TYPE)) {
                arrayObj = (*env)->NewDoubleArray(env, (jsize) size);
            } else {
                /* Void? */
                PyErr_SetString(PyExc_ValueError, "Unsupported jarray Component type");
                return NULL;
            }
        } else if (PyUnicode_Check(two)) {
            Py_UCS1 typecode = 0;
            if (PyUnicode_READY(two) != 0) {
                return NULL;
            } else if (PyUnicode_GET_LENGTH(two) == 1) {
                if (PyUnicode_KIND((two)) == PyUnicode_1BYTE_KIND) {
                    typecode = PyUnicode_1BYTE_DATA(two)[0];
                }
            }
            /* 0 will fall through like any other invalid character. */
            switch (typecode) {
            case 'z':
                arrayObj = (*env)->NewBooleanArray(env, (jsize) size);
                break;
            case 'b':
                arrayObj = (*env)->NewByteArray(env, (jsize) size);
                break;
            case 'c':
                arrayObj = (*env)->NewCharArray(env, (jsize) size);
                break;
            case 's':
                arrayObj = (*env)->NewShortArray(env, (jsize) size);
                break;
            case 'i':
                arrayObj = (*env)->NewIntArray(env, (jsize) size);
                break;
            case 'j':
                arrayObj = (*env)->NewLongArray(env, (jsize) size);
                break;
            case 'f':
                arrayObj = (*env)->NewFloatArray(env, (jsize) size);
                break;
            case 'd':
                arrayObj = (*env)->NewDoubleArray(env, (jsize) size);
                break;
            } // switch
            if (!arrayObj) {
                PyErr_SetString(PyExc_ValueError,
                                "bad typecode (must be z, b, c, s, i, j, f or d)");
                return NULL;
            }
        } else {
            PyErr_SetString(PyExc_ValueError, "Unknown arg type: expected "
                            "one of: J<foo>_ID, pyjobject, jarray");
            return NULL;
        }
    } else {
        PyErr_SetString(PyExc_ValueError, "Unknown arg types.");
        return NULL;
    }

    if (process_java_exception(env)) {
        return NULL;
    }

    if (!arrayObj || size < 0) {
        PyErr_SetString(PyExc_ValueError, "Unknown type.");
        return NULL;
    }

    PyObject* result = jobject_As_PyJObject(env, arrayObj, NULL);
    if (result && three) {
        Py_ssize_t i;
        for (i = 0; i < size; i += 1) {
            // TODO repeated conversion is quite inefficient.
            PySequence_SetItem(result, i, three);
        }

    }

    return result;
}
