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
        PyObject* topy = PyObject_GetAttrString(result, "_to_python");
        if (topy != NULL) {
            Py_DECREF(result);
            result = PyObject_CallObject(topy, NULL);
            Py_DECREF(topy);
        } else {
            /* This is exactly what is done in PyObject_HasAttr. */
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
    JNIEnv *env = pyembed_get_env();
    jobject this = ((PyJObject*) self)->object;
    return jstring_As_PyString(env, this);
}

static PyMethodDef pyjstring_As_PyString_def = { "_to_python", pyjstring_As_PyString, METH_NOARGS, "Convert a Java String into a Python string"};

static PyObject* pyjjpyobject_As_PyObject(PyObject* self, PyObject* unused)
{
    JNIEnv *env = pyembed_get_env();
    jobject this = ((PyJObject*) self)->object;
    return JPyObject_As_PyObject(env, this);
}

static PyMethodDef pyjjpyobject_As_PyObject_def = { "_to_python", pyjjpyobject_As_PyObject, METH_NOARGS, "Unwrap a PyJObject"};

static PyObject* pyjproxy_As_PyObject(PyObject* self, PyObject* unused)
{
    JNIEnv *env = pyembed_get_env();
    jobject this = ((PyJObject*) self)->object;
    jobject jpyObject = jep_Proxy_getPyObject(env, this);
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

/*
 * Set the _to_python method for a type to a reference to an existing method.
 * Very useful for the primitive wrapper types.
 */
int set_to_python_from_method(JNIEnv* env, jclass clazz,
                              const char* methodName)
{
    PyObject* t = (PyObject*) PyJType_Get(env, clazz);
    if (!t) {
        return -1;
    }
    PyObject* a = PyObject_GetAttrString(t, methodName);
    if (!a) {
        Py_DECREF(t);
        return -1;
    }
    int result = PyObject_SetAttrString(t, "_to_python", a);
    Py_DECREF(a);
    Py_DECREF(t);
    return result;
}

int set_to_python_from_method_def(JNIEnv* env, jclass clazz,
                                  PyMethodDef* methodDef)
{
    PyTypeObject* t = PyJType_Get(env, clazz);
    if (!t) {
        return -1;
    }
    PyObject* a = PyDescr_NewMethod(t, methodDef);
    if (!a) {
        Py_DECREF(t);
        return -1;
    }
    int result = PyObject_SetAttrString((PyObject*) t, "_to_python", a);
    Py_DECREF(a);
    Py_DECREF(t);
    return result;
}

int load_conversions(JNIEnv* env)
{
    if (set_to_python_from_method(env, JDOUBLE_OBJ_TYPE, "doubleValue")) {
        return -1;
    }
    if (set_to_python_from_method(env, JFLOAT_OBJ_TYPE, "floatValue")) {
        return -1;
    }
    if (set_to_python_from_method(env, JBYTE_OBJ_TYPE, "byteValue")) {
        return -1;
    }
    if (set_to_python_from_method(env, JSHORT_OBJ_TYPE, "shortValue")) {
        return -1;
    }
    if (set_to_python_from_method(env, JINT_OBJ_TYPE, "intValue")) {
        return -1;
    }
    if (set_to_python_from_method(env, JLONG_OBJ_TYPE, "longValue")) {
        return -1;
    }
    if (set_to_python_from_method(env, JBOOL_OBJ_TYPE, "booleanValue")) {
        return -1;
    }
    if (set_to_python_from_method(env, JCHAR_OBJ_TYPE, "charValue")) {
        return -1;
    }

    if (set_to_python_from_method_def(env, JBIGINTEGER_TYPE,
                                      &jbigint_As_PyLong_def)) {
        return -1;
    }
    if (set_to_python_from_method_def(env, JSTRING_TYPE,
                                      &pyjstring_As_PyString_def)) {
        return -1;
    }
    if (set_to_python_from_method_def(env, JPYOBJECT_TYPE,
                                      &pyjjpyobject_As_PyObject_def)) {
        return -1;
    }
    if (set_to_python_from_method_def(env, JAVA_PROXY_TYPE,
                                      &pyjproxy_As_PyObject_def)) {
        return -1;
    }

    return 0;
}
