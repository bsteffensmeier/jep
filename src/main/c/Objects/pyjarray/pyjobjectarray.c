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

   This contains the definition of the Python type for Java Object[]. This
   implements the functions necessary for Object[] to behjave like a typical
   Python sequence.

   The structure of this file is nearly identical to pyjprimitivearray_template
   but the template can not be reused because the JNI interface for objects is
   much different. However, if there are any significant changes in this file it
   is likely that pyjprimitivearray_template.c will require similar changes.

*/

#include "Jep.h"

static PyObject* pyjobjectarray_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    // TODO add args for elementClass and initialElement
    static char *kwlist[] = {"size", NULL};
    JNIEnv *env  = pyembed_get_env();
    int len;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &len)) {
        return NULL;
    }
    // TODO make this work for subtypes
    jobjectArray n = (*env)->NewObjectArray(env, len, JOBJECT_TYPE, NULL);
    if (!n) {
        process_java_exception(env);
        return NULL;
    }   
    return PyJObject_New(env, type, n, NULL);
}

static Py_ssize_t pyjobjectarray_length(PyJObject *o)
{
    JNIEnv *env  = pyembed_get_env();
    return (*env)->GetArrayLength(env, o->object);
}

static PyObject* pyjobjectarray_concat(PyJObject *a, PyObject *pb)
{
    if (!PyJObject_Check(pb)){
        PyErr_Format(PyExc_TypeError,
             "can only append jobjectarray (not \"%.200s\")", Py_TYPE(pb)->tp_name);
        return NULL;
    }
    JNIEnv *env  = pyembed_get_env();
    PyJObject* b = (PyJObject*) pb;
    if (!(*env)->IsAssignableFrom(env, b->clazz, JOBJECT_ARRAY_TYPE)) {
        PyErr_Format(PyExc_TypeError,
             "can only append jobjectarray (not \"%.200s\")", Py_TYPE(pb)->tp_name);
        return NULL;
    }
    jsize a_len = (*env)->GetArrayLength(env, a->object);
    jsize b_len = (*env)->GetArrayLength(env, b->object);
    jsize n_len = a_len + b_len;
    PyTypeObject* n_type;
    jobjectArray n;
    if ((*env)->IsAssignableFrom(env, b->clazz, a->clazz)) {
        jclass compClazz = java_lang_Class_getComponentType(env, a->clazz);
        n = (*env)->NewObjectArray(env, n_len, compClazz, NULL);
        n_type = Py_TYPE((PyObject*) a);
        Py_INCREF(n_type);
        (*env)->DeleteLocalRef(env, compClazz);
    } else if ((*env)->IsAssignableFrom(env, a->clazz, b->clazz)) {
        jclass compClazz = java_lang_Class_getComponentType(env, b->clazz);
        n = (*env)->NewObjectArray(env, n_len, compClazz, NULL);
        n_type = Py_TYPE((PyObject*) b);
        Py_INCREF(n_type);
        (*env)->DeleteLocalRef(env, compClazz);
    } else {
        n_type = PyJType_Get(env, JOBJECT_ARRAY_TYPE);
        if (!n_type) {
            return NULL;
        }
        n = (*env)->NewObjectArray(env, n_len, JOBJECT_TYPE, NULL);
    }
    if (n == NULL) {
        process_java_exception(env);
        return NULL;
    }
    Py_ssize_t i;
    for (i = 0; i < a_len; i += 1) {
        jobject element = (*env)->GetObjectArrayElement(env, a->object, i);
        (*env)->SetObjectArrayElement(env, n, i, element);
        (*env)->DeleteLocalRef(env, element);
    }
    for (i = 0; i < b_len; i += 1) {
        jobject element = (*env)->GetObjectArrayElement(env, b->object, i);
        (*env)->SetObjectArrayElement(env, n, a_len + i, element);
        (*env)->DeleteLocalRef(env, element);
    }
    PyObject* result = PyJObject_New(env, n_type, n, a->clazz);
    Py_DECREF(n_type);
    return result;
}

static PyObject* pyjobjectarray_repeat(PyJObject *o, Py_ssize_t n)
{
    JNIEnv *env  = pyembed_get_env();
    jsize o_len = (*env)->GetArrayLength(env, o->object);
    jsize n_len = o_len * n;
    jclass compClazz = java_lang_Class_getComponentType(env, o->clazz);
    jobjectArray np = (*env)->NewObjectArray(env, n_len, compClazz, NULL);
    (*env)->DeleteLocalRef(env, compClazz);
    if (np == NULL) {
        process_java_exception(env);
        return NULL;
    }

    Py_ssize_t i, j;
    for (i = 0; i < o_len; i += 1) {
        jobject element = (*env)->GetObjectArrayElement(env, o->object, i % o_len);
        for (j = i; j < n_len; j += o_len) {
            (*env)->SetObjectArrayElement(env, np, j, element);
        }
        (*env)->DeleteLocalRef(env, element);
    }

    PyObject* result = PyJObject_New(env, Py_TYPE((PyObject*) o), np, o->clazz);
    (*env)->DeleteLocalRef(env, np);
    return result;
}

static PyObject* pyjobjectarray_item(PyJObject *o, Py_ssize_t i)
{
    JNIEnv *env = pyembed_get_env();
    jobject element = (*env)->GetObjectArrayElement(env, o->object, i);
    if (process_java_exception(env)) {
        return NULL;
    }
    PyObject* result = jobject_As_PyObject(env, element);
    (*env)->DeleteLocalRef(env, element);
    return result;
}

static int pyjobjectarray_ass_item(PyJObject *o, Py_ssize_t i, PyObject *v)
{
    JNIEnv *env = pyembed_get_env();
    jclass compClazz = java_lang_Class_getComponentType(env, o->clazz);
    jobject element = PyObject_As_jobject(env, v, compClazz);
    (*env)->DeleteLocalRef(env, compClazz);
    if (PyErr_Occurred()) {
        return -1;
    }
    (*env)->SetObjectArrayElement(env, o->object, i, element);
    (*env)->DeleteLocalRef(env, element);
    if (process_java_exception(env)) {
        return -1;
    }
    return 0;
}

static int pyjobjectarray_contains(PyJObject *o, PyObject *v)
{
    JNIEnv *env = pyembed_get_env();
    jclass compClazz = java_lang_Class_getComponentType(env, o->clazz);
    jobject value = PyObject_As_jobject(env, v, compClazz);
    (*env)->DeleteLocalRef(env, compClazz);
    if(PyErr_Occurred()) {
        /* 
	 * Objects that can't be converted to the java
	 * type are not in the array 
	 */
	PyErr_Clear();
        return 0;
    }
    int result = 0;
    jsize len = (*env)->GetArrayLength(env, o->object);

    Py_ssize_t i;
    for (i = 0; i < len; i += 1) {
        jobject element = (*env)->GetObjectArrayElement(env, o->object, i);
        jboolean eq = java_lang_Object_equals(env, value, element);
        (*env)->DeleteLocalRef(env, element);
        if (process_java_exception(env)) {
            result = -1;
	    break;
        }
        if (eq) {
            result = 1;
            break;
        }
    }
    (*env)->DeleteLocalRef(env, value);

    return result; 
}

static PyObject* pyjobjectarray_subscript(PyJObject* o, PyObject* index)
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
        jobject element = (*env)->GetObjectArrayElement(env, o->object, i);
        if (process_java_exception(env)) {
            return NULL;
        }
        PyObject* result = jobject_As_PyObject(env, element);
        (*env)->DeleteLocalRef(env, element);
	return result;
    } else if (PySlice_Check(index)) {
        Py_ssize_t start, stop, step, slicelength, i;
        jobjectArray result;

        if (PySlice_Unpack(index, &start, &stop, &step) < 0) {
            return NULL;
        }
        JNIEnv *env  = pyembed_get_env();
        jclass compClazz = java_lang_Class_getComponentType(env, o->clazz);
        jsize len = (*env)->GetArrayLength(env, o->object);
        slicelength = PySlice_AdjustIndices(len, &start, &stop, step);
        result = (*env)->NewObjectArray(env, slicelength, compClazz, NULL);
        (*env)->DeleteLocalRef(env, compClazz);
        if (result == NULL){
            process_java_exception(env);
            return NULL;
        }

        for (i = 0; i < slicelength; i += 1) {
            jobject element = (*env)->GetObjectArrayElement(env, o->object, start + i*step);
            (*env)->SetObjectArrayElement(env, result, i, element);
            (*env)->DeleteLocalRef(env, element);
        }
        return PyJObject_New(env, Py_TYPE((PyObject*) o), result, o->clazz);
    } else {
        PyErr_Format(PyExc_TypeError,
                     "JArray indices must be integers or slices, not %.200s",
                      Py_TYPE(index)->tp_name);
        return NULL;
    }
}

static int pyjobjectarray_ass_subscript(PyJObject* o, PyObject* index, PyObject* values)
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
        jclass compClazz = java_lang_Class_getComponentType(env, o->clazz);
        jobject element = PyObject_As_jobject(env, values, compClazz);
        (*env)->DeleteLocalRef(env, compClazz);
        if (PyErr_Occurred()) {
            return -1;
        }
        (*env)->SetObjectArrayElement(env, o->object, i, element);
        (*env)->DeleteLocalRef(env, element);
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
        jclass compClazz = java_lang_Class_getComponentType(env, o->clazz);
        for (cur = start, i = 0; i < slicelength;
            cur += step, i++) {
            PyObject* item = PySequence_GetItem(values, i);
            jobject element = PyObject_As_jobject(env, item, compClazz);
            if (PyErr_Occurred()) {
                return -1;
            }
            (*env)->SetObjectArrayElement(env, o->object, cur, element);
            (*env)->DeleteLocalRef(env, element);
        }
        (*env)->DeleteLocalRef(env, compClazz);
        return 0;
    } else {
        PyErr_Format(PyExc_TypeError,
                     "JArray indices must be integers or slices, not %.200s",
                      Py_TYPE(index)->tp_name);
        return -1;
    }
}

static PyType_Slot pyjobjectarray_slots[] = {
    {Py_tp_doc, "Jep java object array"},
    {Py_tp_new, (void*) pyjobjectarray_new},
    /*
     * **** sequence slots ****
     */
    {Py_sq_length, (void*) pyjobjectarray_length},
    {Py_sq_contains, (void*) pyjobjectarray_contains},
    {Py_sq_concat, (void*) pyjobjectarray_concat},
    {Py_sq_repeat, (void*) pyjobjectarray_repeat},
    {Py_sq_item, (void*) pyjobjectarray_item},
    {Py_sq_ass_item, (void*) pyjobjectarray_ass_item},
    // mapping methods
    {Py_mp_subscript, (void*) pyjobjectarray_subscript},
    {Py_mp_ass_subscript, (void*) pyjobjectarray_ass_subscript},
    {0, NULL},
};
static PyType_Spec pyjobjectarray_spec = {
    .name = "[Ljava.lang.Object;",
    .basicsize = 0,
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .slots = pyjobjectarray_slots,
};

PyTypeObject* PyJObjectArray_InitType(JNIEnv *env) 
{
    PyObject* collectionAbc = PyImport_ImportModule("collections.abc");
    if (!collectionAbc) {
        return NULL;
    }
    PyObject* seq = PyObject_GetAttrString(collectionAbc, "Sequence");
    Py_DECREF(collectionAbc);
    if (!seq) {
        return NULL;
    }
    if (!PyType_Check(seq)) {
        Py_DECREF(seq);
        return NULL;
    }
    PyObject *bases = PyTuple_Pack(2, (PyObject*) &PyJObject_Type, seq);
    Py_DECREF(seq);
    if (!bases) {
        return NULL;
    }
    PyObject *type = PyType_FromSpecWithBases(&pyjobjectarray_spec, bases);
    Py_DECREF(bases);
    if (!type) {
        return NULL;
    }
    return (PyTypeObject*) type;
}
