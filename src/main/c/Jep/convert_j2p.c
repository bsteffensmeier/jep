/*
   jep - Java Embedded Python

   Copyright (c) 2017-2021 JEP AUTHORS.

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
PyObject* jchar_As_PyObject(jchar c)
{
    Py_UCS2 value = (Py_UCS2) c;
    return PyUnicode_FromKindAndData(PyUnicode_2BYTE_KIND, &value, 1);
}

PyObject* jstring_As_PyString(JNIEnv *env, jstring jstr)
{
    PyObject* result;
    const jchar *str = (*env)->GetStringChars(env, jstr, 0);
    jsize size = (*env)->GetStringLength(env, jstr);
    result = PyUnicode_DecodeUTF16((const char*) str, size * 2, NULL, NULL);
    (*env)->ReleaseStringChars(env, jstr, str);
    return result;
}

PyObject* JPyObject_As_PyObject(JNIEnv *env, jobject jobj)
{
    PyObject *ret;
    jlong l = jep_python_PyObject_getPyObject(env, jobj);
    ret = (PyObject*) l;
    Py_INCREF(ret);
    return ret;
}

PyObject* jobject_As_PyString(JNIEnv *env, jobject jobj)
{
    PyObject   *result;
    jstring     jstr;

    jstr = java_lang_Object_toString(env, jobj);
    if (process_java_exception(env)) {
        return NULL;
    } else if (jstr == NULL) {
        Py_RETURN_NONE;
    }
    result = jstring_As_PyString(env, jstr);
    (*env)->DeleteLocalRef(env, jstr);
    return result;
}

PyObject* jobject_As_PyJObject(JNIEnv *env, jobject jobj, jclass class)
{
    if (jobj == NULL) {
        Py_RETURN_NONE;
    }
    if (!class) {
        class = (*env)->GetObjectClass(env, jobj);
    }
    if ((*env)->IsSameObject(env, class, JCLASS_TYPE)) {
        return PyJClass_Wrap(env, jobj);
    }
    PyTypeObject* type = PyJType_Get(env, class);
    if (!type) {
        return NULL;
    }
    PyObject* result = PyJObject_New(env, type, jobj, class);
    Py_DECREF(type);
    if (result) {
        // TODO GetAttr would be faster than GetAttrString
        PyObject* topy = PyObject_GetAttrString(result, "_to_python");
        if (topy != NULL) {
            Py_DECREF(result);
            // TODO Don't make a new tuple all the time.
            PyObject* args = PyTuple_New(0);
            result = PyObject_Call(topy, args, NULL);
            Py_DECREF(args);
            Py_DECREF(topy);
        } else {
            PyErr_Clear();
        }
    }
    return result;
}

PyObject* jobject_As_PyObject(JNIEnv *env, jobject jobj)
{
    PyObject* result = NULL;
    jclass    class  = NULL;
    if (jobj == NULL) {
        Py_RETURN_NONE;
    }
    class = (*env)->GetObjectClass(env, jobj);
    jboolean array = java_lang_Class_isArray(env, class);
    if ((*env)->ExceptionCheck(env)) {
        process_java_exception(env);
    } else if (array) {
        result = pyjarray_new(env, jobj);
    } else {
        result = jobject_As_PyJObject(env, jobj, class);
    }
    (*env)->DeleteLocalRef(env, class);
    return result;
}

static PyObject* jbigint_As_PyLong(PyObject* self, PyObject* unused)
{
    PyObject* pystr = PyObject_Str(self);
    if (pystr == NULL) {
        return NULL;
    }
    PyObject* pyint = PyLong_FromUnicodeObject(pystr, 10);
    Py_DECREF(pystr);
    return pyint;
}

static PyMethodDef jbigint_As_PyLong_def = { "_to_python", jbigint_As_PyLong, METH_NOARGS, "Convert a Java BigInteger into a Python int"};

static PyObject* pyjstring_As_PyString(PyObject* self, PyObject* unused)
{
    if (!PyType_IsSubtype(Py_TYPE(self), &PyJObject_Type)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on non-java object.");
        return NULL;
    }
    PyJObject* this = (PyJObject*) self;
    if (this->object == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on class object.");
        return NULL;
    }
    JNIEnv *env = pyembed_get_env();
    if (!(*env)->IsSameObject(env, this->clazz, JSTRING_TYPE)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion, expected String.");
        return NULL;
    }
    return jstring_As_PyString(env, this->object);
}

static PyMethodDef pyjstring_As_PyString_def = { "_to_python", pyjstring_As_PyString, METH_NOARGS, "Convert a Java String into a Python string"};

static PyObject* pyjjpyobject_As_PyObject(PyObject* self, PyObject* unused)
{
    if (!PyType_IsSubtype(Py_TYPE(self), &PyJObject_Type)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on non-java object.");
        return NULL;
    }
    PyJObject* this = (PyJObject*) self;
    if (this->object == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on class object.");
        return NULL;
    }
    JNIEnv *env = pyembed_get_env();
    if (!(*env)->IsSameObject(env, this->clazz, JPYOBJECT_TYPE)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion, expected PyJObject.");
        return NULL;
    }
    return JPyObject_As_PyObject(env, this->object);
}

static PyMethodDef pyjjpyobject_As_PyObject_def = { "_to_python", pyjjpyobject_As_PyObject, METH_NOARGS, "Unwrap a PyJObject"};

#if JEP_NUMPY_ENABLED
static PyObject* pyjndarray_As_PyNDArray(PyObject* self, PyObject* unused)
{
    if (!PyType_IsSubtype(Py_TYPE(self), &PyJObject_Type)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on non-java object.");
        return NULL;
    }
    PyJObject* this = (PyJObject*) self;
    if (this->object == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on class object.");
        return NULL;
    }
    JNIEnv *env = pyembed_get_env();
    if (!jndarray_check(env, this->object)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion, expected NDArray.");
        return NULL;
    }
    return convert_jndarray_pyndarray(env, this->object);
}

static PyMethodDef pyjndarray_As_PyNDArray_def = { "_to_python", pyjndarray_As_PyNDArray, METH_NOARGS, "Convert a Java NDArray to a Python NDArray"};

static PyObject* pyjdndarray_As_PyNDArray(PyObject* self, PyObject* unused)
{
    if (!PyType_IsSubtype(Py_TYPE(self), &PyJObject_Type)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on non-java object.");
        return NULL;
    }
    PyJObject* this = (PyJObject*) self;
    if (this->object == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on class object.");
        return NULL;
    }
    JNIEnv *env = pyembed_get_env();
    if (!jdndarray_check(env, this->object)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion, expected DirectNDArray.");
        return NULL;
    }
    Py_INCREF(self);
    return convert_jdndarray_pyndarray(env, self);
}

static PyMethodDef pyjdndarray_As_PyNDArray_def = { "_to_python", pyjdndarray_As_PyNDArray, METH_NOARGS, "Convert a Java DirectNDArray to a Python NDArray"};
#endif

static PyObject* pyjproxy_As_PyObject(PyObject* self, PyObject* unused)
{
    if (!PyType_IsSubtype(Py_TYPE(self), &PyJObject_Type)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on non-java object.");
        return NULL;
    }
    PyJObject* this = (PyJObject*) self;
    if (this->object == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion on class object.");
        return NULL;
    }
    JNIEnv *env = pyembed_get_env();
    if (!(*env)->IsAssignableFrom(env, this->clazz, JAVA_PROXY_TYPE)) {
        PyErr_SetString(PyExc_TypeError,
                        "Invalid Java->Python conversion, expected Proxy.");
        return NULL;
    }
    jobject jpyObject = jep_Proxy_getPyObject(env, this->object);
    if (jpyObject) {
        return JPyObject_As_PyObject(env, jpyObject);
    } else if ((*env)->ExceptionCheck(env)) {
        process_java_exception(env);
        return NULL;
    } else {
        /* If this is a proxy that wasn't made by jep then do no unwrapping. */
        return self;
    }
}

static PyMethodDef pyjproxy_As_PyObject_def = { "_to_python", pyjproxy_As_PyObject, METH_NOARGS, "Unwrap a java Proxy."};

int load_conversions(JNIEnv* env)
{
    // TODO Error checking and
    PyObject* t = (PyObject*) PyJType_Get(env, JDOUBLE_OBJ_TYPE);
    PyObject* a = PyObject_GetAttrString(t, "doubleValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    t = (PyObject*) PyJType_Get(env, JFLOAT_OBJ_TYPE);
    a = PyObject_GetAttrString(t, "floatValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    t = (PyObject*) PyJType_Get(env, JBYTE_OBJ_TYPE);
    a = PyObject_GetAttrString(t, "byteValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    t = (PyObject*) PyJType_Get(env, JSHORT_OBJ_TYPE);
    a = PyObject_GetAttrString(t, "shortValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    t = (PyObject*) PyJType_Get(env, JINT_OBJ_TYPE);
    a = PyObject_GetAttrString(t, "intValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    t = (PyObject*) PyJType_Get(env, JLONG_OBJ_TYPE);
    a = PyObject_GetAttrString(t, "longValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    t = (PyObject*) PyJType_Get(env, JBOOL_OBJ_TYPE);
    a = PyObject_GetAttrString(t, "booleanValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    t = (PyObject*) PyJType_Get(env, JCHAR_OBJ_TYPE);
    a = PyObject_GetAttrString(t, "charValue");
    PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(t);
    Py_DECREF(a);

    PyTypeObject* type = PyJType_Get(env, JBIGINTEGER_TYPE);
    a = PyDescr_NewMethod(type, &jbigint_As_PyLong_def);
    PyObject_SetAttrString((PyObject*) type, "_to_python", a);
    Py_DECREF(type);
    Py_DECREF(a);

    type = PyJType_Get(env, JSTRING_TYPE);
    a = PyDescr_NewMethod(type, &pyjstring_As_PyString_def);
    PyObject_SetAttrString((PyObject*) type, "_to_python", a);
    Py_DECREF(type);
    Py_DECREF(a);

    type = PyJType_Get(env, JPYOBJECT_TYPE);
    a = PyDescr_NewMethod(type, &pyjjpyobject_As_PyObject_def);
    PyObject_SetAttrString((PyObject*) type, "_to_python", a);
    Py_DECREF(type);
    Py_DECREF(a);

#if JEP_NUMPY_ENABLED
    type = PyJType_Get(env, JEP_NDARRAY_TYPE);
    a = PyDescr_NewMethod(type, &pyjndarray_As_PyNDArray_def);
    PyObject_SetAttrString((PyObject*) type, "_to_python", a);
    Py_DECREF(type);
    Py_DECREF(a);

    type = PyJType_Get(env, JEP_DNDARRAY_TYPE);
    a = PyDescr_NewMethod(type, &pyjdndarray_As_PyNDArray_def);
    PyObject_SetAttrString((PyObject*) type, "_to_python", a);
    Py_DECREF(type);
    Py_DECREF(a);
#endif

    type = PyJType_Get(env, JAVA_PROXY_TYPE);
    a = PyDescr_NewMethod(type, &pyjproxy_As_PyObject_def);
    PyObject_SetAttrString((PyObject*) type, "_to_python", a);
    Py_DECREF(type);
    Py_DECREF(a);

    return 0;
}
