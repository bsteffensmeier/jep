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

typedef struct {
    PyObject_HEAD
    jsize index;
    jobjectArray array;
} pyjobjectarray_iter_object;

static void pyjobjectarray_iter_dealloc(pyjobjectarray_iter_object *o)
{
    JNIEnv *env = pyembed_get_env();
    (*env)->DeleteGlobalRef(env, o->array);
}

static PyObject* pyjobjectarray_iter_next(pyjobjectarray_iter_object *o)
{
    JNIEnv *env = pyembed_get_env();
    jsize length = (*env)->GetArrayLength(env, o->array);
    if (o->index >= length) {
        return NULL;
    }

    jobject element = (*env)->GetObjectArrayElement(env, o->array, o->index);
    if (process_java_exception(env)) {
        return NULL;
    }
    o->index += 1;
    PyObject* result = jobject_As_PyObject(env, element);
    (*env)->DeleteLocalRef(env, element);
    return result;
}

static PyType_Slot pyjobjectarray_iter_slots[] = {
    {Py_tp_doc, "Jep java object array iterator"},
    {Py_tp_dealloc, (void*) pyjobjectarray_iter_dealloc},
    {Py_tp_iter, (void*) PyObject_SelfIter},
    {Py_tp_iternext, (void*) pyjobjectarray_iter_next},
    {0, NULL},
};
PyType_Spec pyjobjectarray_iter_spec = {
    .name = "[java.lang.Object.iterator",
    .basicsize = sizeof(pyjobjectarray_iter_object),
    .flags = Py_TPFLAGS_DEFAULT,
    .slots = pyjobjectarray_iter_slots,
};

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

static pyjobjectarray_iter_object* pyjobjectarray_getiter(PyJObject *a) {
    JNIEnv *env  = pyembed_get_env();
    PyObject *type = (PyObject*) Py_TYPE(a);
    PyTypeObject* itertype = (PyTypeObject*) PyObject_GetAttrString(type, "__itertype__");
    pyjobjectarray_iter_object *iter = PyObject_New(pyjobjectarray_iter_object, itertype);
    if (iter) {
        iter->array = (*env)->NewGlobalRef(env, a->object);
        iter->index = 0;
    }
    return iter;
}

static Py_ssize_t pyjobjectarray_length(PyJObject *o)
{
    JNIEnv *env  = pyembed_get_env();
    return (*env)->GetArrayLength(env, o->object);
}

static PyObject* pyjobjectarray_concat(PyJObject *a, PyObject *pb)
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
    // TODO JOBJECT_TYPE
    // TODO Arrays.copyOf or System.arraycopy?
    jobjectArray n = (*env)->NewObjectArray(env, n_len, JOBJECT_TYPE, NULL);
    if (n == NULL) {
        process_java_exception(env);
        return NULL;
    }
    if (a_len > 0) {
        Py_ssize_t i;
        for (i = 0; i < a_len; i += 1) {
            jobject element = (*env)->GetObjectArrayElement(env, a->object, i);
            (*env)->SetObjectArrayElement(env, n, i, element);
            (*env)->DeleteLocalRef(env, element);
        }
    }
    if (b_len > 0) {
        Py_ssize_t i;
        for (i = 0; i < a_len; i += 1) {
            jobject element = (*env)->GetObjectArrayElement(env, b->object, i);
            (*env)->SetObjectArrayElement(env, n, a_len + i, element);
            (*env)->DeleteLocalRef(env, element);
        }
    }
    return PyJObject_New(env, Py_TYPE((PyObject*) a), n, a->clazz);
}

static PyObject* pyjobjectarray_repeat(PyJObject *o, Py_ssize_t n)
{
    JNIEnv *env  = pyembed_get_env();
    jsize o_len = (*env)->GetArrayLength(env, o->object);
    jsize n_len = o_len * n;
    jclass compClazz = java_lang_Class_getComponentType(env, o->clazz);
    jobjectArray np = (*env)->NewObjectArray(env, n_len, compClazz, NULL);
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
    {Py_tp_iter, (void*) pyjobjectarray_getiter},
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

PyTypeObject* PyJObjectArray_InitType(JNIEnv *env) {
    /* TODO Starting in 3.10 bases can be a single type so there will be no need to make a tuple. */
    PyObject *bases = PyTuple_Pack(1, (PyObject*) &PyJObject_Type);
    if (!bases) {
        return NULL;
    }
    PyObject *type = PyType_FromSpecWithBases(&pyjobjectarray_spec, bases);
    Py_DECREF(bases);
    if (!type) {
        return NULL;
    }
    PyObject* iterType = PyType_FromSpec(&pyjobjectarray_iter_spec);
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
