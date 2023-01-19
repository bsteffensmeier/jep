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

   This is a template file that can be used with macro definitions to generate 
   a type for any of the java primitive arrays. A template is used because the
   code for all the primitive array types can use the exact same logic, however
   because the different primitives are different sizes, use different JNI
   functions, and need different conversion functions it is necessary to have
   different compiled functions for each primitive array type.

   The following macro definitions must be defined before including this
   template. Except for the first three the macro definition should point to an
   externally defined type, variable, or function. In general the correct value
   is obtained by simply replacing 'primitive' with the actual name of the
   primitive.

   PyJPrimitiveArray_InitType
     - The name of the publuc function that initializes and returns the type
   ARRAY_TP_NAME
     - The name of the type, used in the tp_name slot
   ARRAY_TP_DOC
     - The documentation for the type, used in the tp_doc slot
   jprimitive
     - The JNI type of the primitive
   jprimitiveArray
     - The JNI type of the primitive array
   JPRIMITIVE_ARRAY_TYPE
     - The cached jclass for the array type
   jprimitive_As_PyObject
     - The name of the function for converting from a JNI primitive to a
       PyObject
   PyObject_As_jprimitive
     - The name of the function for converting from a PyObject to a JNI
       primitive
   NewPrimitiveArray
     - The JNI function for creating a new array
   GetPrimitiveArrayElements
     - The JNI function for obtaining array elements
   ReleasePrimitiveArrayElements
     - The JNI function for releasing array elements
   GetPrimitiveArrayRegion
     - The JNI function for obtaining an array region
   SetPrimitiveArrayRegion
     - The JNI function for assigning an array region

   The structure of this file is nearly identical to pyjobjectarray but the
   template can not be reused because the JNI interface for objects is much
   different. However, if there are any significant changes in this file it
   is likely that pyjobjectarray.c will require similar changes.
*/

#ifdef jprimitive

#include "Jep.h"

typedef struct {
    PyObject_HEAD
    jsize index;
    jprimitiveArray array;
} pyjprimitivearray_iter_object;

static void pyjprimitivearray_iter_dealloc(pyjprimitivearray_iter_object *o)
{
    JNIEnv *env = pyembed_get_env();
    (*env)->DeleteGlobalRef(env, o->array);
}

static PyObject* pyjprimitivearray_iter_next(pyjprimitivearray_iter_object *o)
{
    JNIEnv *env = pyembed_get_env();
    jsize length = (*env)->GetArrayLength(env, o->array);
    if (o->index >= length) {
        return NULL;
    }

    jprimitive element;
    (*env)->GetPrimitiveArrayRegion(env, o->array, o->index, 1, &element);
    if (process_java_exception(env)) {
        return NULL;
    }
    o->index += 1;
    return jprimitive_As_PyObject(element);
}

static PyType_Slot pyjprimitivearray_iter_slots[] = {
    {Py_tp_doc, ARRAY_TP_DOC " iterator"},
    {Py_tp_dealloc, (void*) pyjprimitivearray_iter_dealloc},
    {Py_tp_iter, (void*) PyObject_SelfIter},
    {Py_tp_iternext, (void*) pyjprimitivearray_iter_next},
    {0, NULL},
};
static PyType_Spec pyjprimitivearray_iter_spec = {
    .name = ARRAY_TP_NAME ".iterator",
    .basicsize = sizeof(pyjprimitivearray_iter_object),
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = pyjprimitivearray_iter_slots,
};

static PyObject* pyjprimitivearray_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"size", NULL};
    JNIEnv *env  = pyembed_get_env();
    int len;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &len)) {
        return NULL;
    }
    jprimitiveArray n = (*env)->NewPrimitiveArray(env, len);
    if (!n) {
        process_java_exception(env);
        return NULL;
    }   
    return PyJObject_New(env, type, n, JPRIMITIVE_ARRAY_TYPE);
}

static pyjprimitivearray_iter_object* pyjprimitivearray_getiter(PyJObject *a) {
    JNIEnv *env  = pyembed_get_env();
    PyObject *type = (PyObject*) Py_TYPE(a);
    PyTypeObject* itertype = (PyTypeObject*) PyObject_GetAttrString(type, "__itertype__");
    pyjprimitivearray_iter_object *iter = PyObject_New(pyjprimitivearray_iter_object, itertype);
    if (iter) {
        iter->array = (*env)->NewGlobalRef(env, a->object);
        iter->index = 0;
    }
    return iter;
}

static Py_ssize_t pyjprimitivearray_length(PyJObject *o)
{
    JNIEnv *env  = pyembed_get_env();
    return (*env)->GetArrayLength(env, o->object);
}

static PyObject* pyjprimitivearray_concat(PyJObject *a, PyObject *pb)
{
    if (Py_TYPE((PyObject*) a) != Py_TYPE(pb)) {
        PyErr_Format(PyExc_TypeError,
             "can only append array of same type (not \"%.200s\")",
                 Py_TYPE(pb)->tp_name);
        return NULL;
    }
    PyJObject* b = (PyJObject*) pb;
    JNIEnv *env  = pyembed_get_env();
    jsize a_len = (*env)->GetArrayLength(env, a->object);
    jsize b_len = (*env)->GetArrayLength(env, b->object);
    jsize n_len = a_len + b_len;
    jprimitiveArray n = (*env)->NewPrimitiveArray(env, n_len);
    if (n == NULL) {
        process_java_exception(env);
        return NULL;
    }
    if (a_len > 0) {
        jprimitive *a_data = (*env)->GetPrimitiveArrayElements(env, a->object, 0);
        if (a_data == NULL) {
            process_java_exception(env);
            return NULL;
        }
        (*env)->SetPrimitiveArrayRegion(env, n, 0, a_len, a_data);
        (*env)->ReleasePrimitiveArrayElements(env, a->object, a_data, JNI_ABORT);
    }
    if (b_len > 0) {
        jprimitive *b_data = (*env)->GetPrimitiveArrayElements(env, b->object, 0);
        if (b_data == NULL) {
            process_java_exception(env);
            return NULL;
        }
        (*env)->SetPrimitiveArrayRegion(env, n, a_len, b_len, b_data);
        (*env)->ReleasePrimitiveArrayElements(env, b->object, b_data, JNI_ABORT);
    }
    return PyJObject_New(env, Py_TYPE((PyObject*) a), n, JPRIMITIVE_ARRAY_TYPE);
}

static PyObject* pyjprimitivearray_repeat(PyJObject *o, Py_ssize_t n)
{
    JNIEnv *env  = pyembed_get_env();
    jsize o_len = (*env)->GetArrayLength(env, o->object);
    jsize n_len = o_len * n;
    jprimitiveArray np = (*env)->NewPrimitiveArray(env, n_len);
    if (np == NULL) {
        process_java_exception(env);
        return NULL;
    }
    jprimitive *o_data = (*env)->GetPrimitiveArrayElements(env, o->object, 0);
    if (o_data == NULL) {
        process_java_exception(env);
        return NULL;
    }

    Py_ssize_t i;
    for (i = 0; i < n_len; i += 1) {
        (*env)->SetPrimitiveArrayRegion(env, o->object, i*o_len, o_len, o_data);
    }

    (*env)->ReleasePrimitiveArrayElements(env, o->object, o_data, JNI_ABORT);
    return PyJObject_New(env, Py_TYPE((PyObject*) o), np, JPRIMITIVE_ARRAY_TYPE);
}

static PyObject* pyjprimitivearray_item(PyJObject *o, Py_ssize_t i)
{
    JNIEnv *env = pyembed_get_env();
    jprimitive element;
    (*env)->GetPrimitiveArrayRegion(env, o->object, i, 1, &element);
    if (process_java_exception(env)) {
        return NULL;
    }
    return jprimitive_As_PyObject(element);
}

static int pyjprimitivearray_ass_item(PyJObject *o, Py_ssize_t i, PyObject *v)
{
    JNIEnv *env = pyembed_get_env();
    jprimitive element = PyObject_As_jprimitive(v);
    if (PyErr_Occurred()) {
        return -1;
    }
    (*env)->SetPrimitiveArrayRegion(env, o->object, i, 1, &element);
    if (process_java_exception(env)) {
        return -1;
    }
    return 0;
}

static int pyjprimitivearray_contains(PyJObject *o, PyObject *v)
{
    jprimitive value = PyObject_As_jprimitive(v);
    if(PyErr_Occurred()) {
        /* 
	 * Objects that can't be converted to the java
	 * primitive type are not in the array 
	 */
	PyErr_Clear();
        return 0;
    }
    int result = 0;
    JNIEnv *env  = pyembed_get_env();
    jsize len = (*env)->GetArrayLength(env, o->object);
    jprimitive *data = (*env)->GetPrimitiveArrayCritical(env, o->object, 0);

    Py_ssize_t i;
    for (i = 0; i < len; i += 1) {
        if (data[i] == value) {
            result = 1;
            break;
        }
    }

    (*env)->ReleasePrimitiveArrayCritical(env, o->object, data, JNI_ABORT);
    return result; 
}

static PyObject* pyjprimitivearray_subscript(PyJObject* o, PyObject* index)
{
    if (PyIndex_Check(index)) {
        Py_ssize_t i = PyNumber_AsSsize_t(index, PyExc_IndexError);
        if (i==-1 && PyErr_Occurred()) {
            return NULL;
        }
        JNIEnv *env  = pyembed_get_env();
        if (i < 0) {
            i += (*env)->GetArrayLength(env, o->object);
        }
        jprimitive element;
        (*env)->GetPrimitiveArrayRegion(env, o->object, i, 1, &element);
        if (process_java_exception(env)) {
            return NULL;
        }
        return jprimitive_As_PyObject(element);
    } else if (PySlice_Check(index)) {
        Py_ssize_t start, stop, step, slicelength, cur, i;
        jprimitiveArray result;

        if (PySlice_Unpack(index, &start, &stop, &step) < 0) {
            return NULL;
        }
        JNIEnv *env  = pyembed_get_env();
        jsize len = (*env)->GetArrayLength(env, o->object);
        slicelength = PySlice_AdjustIndices(len, &start, &stop, step);
        result = (*env)->NewPrimitiveArray(env, slicelength);
        if (result == NULL){
            process_java_exception(env);
            return NULL;
        }

        if (slicelength <= 0) {
            // Don't need to copy any data
        } else if (step == 1) {
            jprimitive *data = (*env)->GetPrimitiveArrayElements(env, result, JNI_FALSE);
	    (*env)->GetPrimitiveArrayRegion(env, o->object, start, slicelength, data);
            (*env)->ReleasePrimitiveArrayElements(env, result, data, JNI_COMMIT);
        } else {
            jprimitive *data = (*env)->GetPrimitiveArrayElements(env, result, 0);

            for (cur = start, i = 0; i < slicelength;
                 cur += step, i++) {
	        (*env)->GetPrimitiveArrayRegion(env, o->object, cur, 1, data + i);
            }
            (*env)->ReleasePrimitiveArrayElements(env, result, data, JNI_COMMIT);
        }
        return PyJObject_New(env, Py_TYPE((PyObject*) o), result, JPRIMITIVE_ARRAY_TYPE);
    } else {
        PyErr_Format(PyExc_TypeError,
                     "JArray indices must be integers or slices, not %.200s",
                      Py_TYPE(index)->tp_name);
        return NULL;
    }
}

static int pyjprimitivearray_ass_subscript(PyJObject* o, PyObject* index, PyObject* values)
{
    if (PyIndex_Check(index)) {
        Py_ssize_t i = PyNumber_AsSsize_t(index, PyExc_IndexError);
        if (i==-1 && PyErr_Occurred()) {
            return -1;
        }
        JNIEnv *env  = pyembed_get_env();
        if (i < 0) {
            i += (*env)->GetArrayLength(env, o->object);
        }
        jprimitive element = PyObject_As_jprimitive(values);
        if (PyErr_Occurred()) {
            return -1;
        }
        (*env)->SetPrimitiveArrayRegion(env, o->object, i, 1, &element);
        if (process_java_exception(env)) {
            return -1;
        }
        return 0;
    } else if (PySlice_Check(index)) {
        Py_ssize_t start, stop, step, slicelength, cur, i;

        if (!PySequence_Check(values)) {
            PyErr_Format(PyExc_TypeError,
                         "JArray can only slice assign a sequence");
            return -1;
        }
        if (PySlice_Unpack(index, &start, &stop, &step) < 0) {
            return -1;
        }
        JNIEnv *env  = pyembed_get_env();
        jsize len = (*env)->GetArrayLength(env, o->object);
        slicelength = PySlice_AdjustIndices(len, &start, &stop, step);

        if (slicelength != PySequence_Size(values)) {
            PyErr_Format(PyExc_TypeError,
                         "JArray can only slice assign a sequence of matching length");
            return -1;

        }

        for (cur = start, i = 0; i < slicelength;
            cur += step, i++) {
            PyObject* item = PySequence_GetItem(values, i);
            jprimitive element = PyObject_As_jprimitive(item);
            if (PyErr_Occurred()) {
                return -1;
            }
            (*env)->SetPrimitiveArrayRegion(env, o->object, cur, 1, &element);
        }
        return 0;
    } else {
        PyErr_Format(PyExc_TypeError,
                     "JArray indices must be integers or slices, not %.200s",
                      Py_TYPE(index)->tp_name);
        return -1;
    }
}

static PyType_Slot pyjprimitivearray_slots[] = {
    {Py_tp_doc, ARRAY_TP_DOC},
    {Py_tp_new, (void*) pyjprimitivearray_new},
    {Py_tp_iter, (void*) pyjprimitivearray_getiter},
    /*
     * **** sequence slots ****
     */
    {Py_sq_length, (void*) pyjprimitivearray_length},
    {Py_sq_contains, (void*) pyjprimitivearray_contains},
    {Py_sq_concat, (void*) pyjprimitivearray_concat},
    {Py_sq_repeat, (void*) pyjprimitivearray_repeat},
    {Py_sq_item, (void*) pyjprimitivearray_item},
    {Py_sq_ass_item, (void*) pyjprimitivearray_ass_item},
    // mapping methods
    {Py_mp_subscript, (void*) pyjprimitivearray_subscript},
    {Py_mp_ass_subscript, (void*) pyjprimitivearray_ass_subscript},
    {0, NULL},
};
static PyType_Spec pyjprimitivearray_spec = {
    .name = ARRAY_TP_NAME,
    .basicsize = 0,
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = pyjprimitivearray_slots,
};

PyTypeObject* PyJPrimitiveArray_InitType(JNIEnv *env) {
    /* TODO Starting in 3.10 bases can be a single type so there will be no need to make a tuple. */
    PyObject *bases = PyTuple_Pack(1, (PyObject*) &PyJObject_Type);
    if (!bases) {
        return NULL;
    }
    PyObject *type = PyType_FromSpecWithBases(&pyjprimitivearray_spec, bases);
    Py_DECREF(bases);
    if (!type) {
        return NULL;
    }
    PyObject* iterType = PyType_FromSpec(&pyjprimitivearray_iter_spec);
    if (!iterType) {
        Py_DECREF(type);
	return NULL;
    }
    if (PyObject_SetAttrString((PyObject*) type, "__itertype__", iterType)) {
        Py_DECREF(type);
        Py_DECREF(iterType);
	return NULL;
    }
    return (PyTypeObject*) type;
}

#endif // ifdef jprimitive 
