/* 
   jep - Java Embedded Python

   Copyright (c) 2004 - 2011 AUTHORS.

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

#ifdef WIN32
# include "winconfig.h"
#endif

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

// shut up the compiler
#ifdef _POSIX_C_SOURCE
#  undef _POSIX_C_SOURCE
#endif
#include <jni.h>

// shut up the compiler
#ifdef _POSIX_C_SOURCE
#  undef _POSIX_C_SOURCE
#endif
#ifdef _FILE_OFFSET_BITS
# undef _FILE_OFFSET_BITS
#endif
#include "Python.h"

#include "pyjtype.h"
#include "pyjobject2.h"
#include "pyembed.h"

/* Look up during Init, save for later. */
jclass jclass_java_lang_Object;

/* Look up during Init, save for later */
jmethodID jmethod_object_toString;
jmethodID jmethod_object_hashCode;
jmethodID jmethod_object_equals;

static PyObject* pyjobject2_new(PyTypeObject*, PyObject*, PyObject*);

static PyObject* pyjobject2_jtopy(JNIEnv*, jvalue);

static jvalue pyjobject2_pytoj(JNIEnv*, PyObject*);

static int pyjobject2_extend(PyJTypeObject* pyjtype){
    pyjtype->pyjtp_pytype.tp_basicsize =  sizeof(PyJObject2Object);
    pyjtype->pyjtp_pytype.tp_new = pyjobject2_new;
    pyjtype->pyjtp_pytype.tp_hash = PyJObject2_GenericHash;
    pyjtype->pyjtp_pytype.tp_str = PyJObject2_GenericStr;
    pyjtype->pyjtp_pytype.tp_richcompare = PyJObject2_GenericCompare;
    pyjtype->pyjtp_pytype.tp_alloc = PyJObject2_GenericAlloc;
    pyjtype->pyjtp_pytoj = pyjobject2_pytoj;
    pyjtype->pyjtp_jtopy = pyjobject2_jtopy;
    return 0;
}


/* 
 * Load global jclass and jmethod objects
 *
 * This function returns 0 if everything loads successfully, and nonzero if
 * anything fails to load. On errors this method sets a Python Exception and
 * does not clear the java exception.
 */
int _PyJObject2_Init(JNIEnv *env) {
    jclass java_lang_object = NULL;

    java_lang_object = (*env)->FindClass(env, "java/lang/Object");
    if(java_lang_object == NULL){
            PyErr_SetString(PyExc_RuntimeError, 
                            "Failed to load java class java.lang.Object");
        return 1;
    }

    jmethod_object_toString = (*env)->GetMethodID(env, java_lang_object, "toString",
                                                  "()Ljava/lang/String;");
    if(jmethod_object_toString == NULL){
        PyErr_SetString(PyExc_RuntimeError, 
                        "Failed to load java method java.lang.Object.toString()");
        return 1;
    }
    jmethod_object_hashCode = (*env)->GetMethodID(env, java_lang_object, "hashCode",
                                                  "()I");
    if(jmethod_object_hashCode == NULL){
        PyErr_SetString(PyExc_RuntimeError, 
                        "Failed to load java method java.lang.Object.hashCode()");
        return 1;
    }
    jmethod_object_equals = (*env)->GetMethodID(env, java_lang_object, "equals",
                                                  "(Ljava/lang/Object;)Z");
    if(jmethod_object_equals == NULL){
        PyErr_SetString(PyExc_RuntimeError, 
                        "Failed to load java method java.lang.Object.equals()");
        return 1;
    }

    jclass_java_lang_Object = (*env)->NewGlobalRef(env,java_lang_object);
    PyJType_Extend("java.lang.Object", pyjobject2_extend);
    return 0;
}

static PyObject* pyjobject2_jtopy(JNIEnv *env, jvalue value) {
    jobject           jobj  = NULL;
    jclass            class = NULL;
    PyTypeObject*     type  = NULL;
    PyObject*         pyobj = NULL;
    jobj = value.l;
    // TODO this should just assume it is makeing a pyjobject2Object
    class = (*env)->GetObjectClass(env, jobj);
    type = (PyTypeObject*) PyJType_jtopy(env, class);
    if(type == NULL){
        return NULL;
    }
    pyobj = type->tp_alloc(type, 0);
    if(pyobj == NULL){
        return NULL;
    }
    ((PyJObject2Object*) pyobj)->object = jobj;
    return pyobj;
}

static jvalue pyjobject2_pytoj(JNIEnv* env, PyObject* pyobj){
    PyJObject2Object* pyjobject = (PyJObject2Object*) pyobj;
    jvalue val;
    val.l = (*env)->NewLocalRef(env, pyjobject->object);
    return val;
}

PyObject* PyJObject2_jtopy(JNIEnv *env, jobject object) {
    jclass class;
    PyJTypeObject* type;
    PyObject* self;
    if(object == NULL) {
        // TODO perhaps this should jtopy the void type?
        Py_INCREF(Py_None);
        return Py_None;
    }
    class = (*env)->GetObjectClass(env, object);
    type = (PyJTypeObject*) PyJType_jtopy(env, class);
    if(type == NULL){
        return NULL;
    }else if(type == &PyJType_Type){
      jvalue value = {.l = object};
      return type->pyjtp_jtopy(env, value);
    }else{
      self = type->pyjtp_pytype.tp_alloc(&(type->pyjtp_pytype), 0);
      ((PyJObject2Object*) self)->object = (*env)->NewGlobalRef(env,object);
      return self;
   }
}

PyObject* PyJObject2_GenericStr(PyObject* self) {
    JNIEnv*     env   = NULL;
    jvalue      jval;
    jstring     jstr  = NULL;
    const char* cstr  = NULL;
    PyObject*   pystr = NULL;
    env = pyembed_get_env();
    // TODO type check and whatnot
    jval = ((PyJTypeObject*) Py_TYPE(self))->pyjtp_pytoj(env, self);
    jstr = (*env)->CallObjectMethod(env, jval.l, jmethod_object_toString);
    if(jstr == NULL){
        if((*env)->ExceptionCheck(env) == JNI_TRUE){
            // TODO wrap.
            PyErr_SetString(PyExc_RuntimeError, "Failed to get java string.");
            return NULL;
        }else{
            Py_RETURN_NONE;
        }
    }
    cstr = (*env)->GetStringUTFChars(env, jstr, NULL);
    if(cstr == NULL){
        PyErr_SetString(PyExc_RuntimeError, "Failed to get java UTF string.");
        return NULL;
    }
    pystr =  PyString_FromString(cstr);
    (*env)->ReleaseStringUTFChars(env, jstr, cstr);
    (*env)->DeleteLocalRef(env, jstr);
    return pystr;
}

long PyJObject2_GenericHash(PyObject* self) {
    JNIEnv* env  = NULL;
    jvalue  jval;
    jint    hash = -1;

    env = pyembed_get_env();
    // TODO type check and whatnot
    jval = ((PyJTypeObject*) Py_TYPE(self))->pyjtp_pytoj(env, self);
    hash = (*env)->CallIntMethod(env, jval.l, jmethod_object_hashCode);
    if((*env)->ExceptionCheck(env) == JNI_TRUE){
        // TODO wrap.
        PyErr_SetString(PyExc_RuntimeError, "Failed to get java string.");
        return -1;
    }
    if(hash == -1) {
        hash = -2;
    }
    return hash;
}

PyObject* PyJObject2_GenericCompare(PyObject* self, PyObject* other, int opid) {
    JNIEnv*        env       = NULL;
    PyJTypeObject* selfType  = NULL;
    PyJTypeObject* otherType = NULL;
    jvalue         jself;
    jvalue         jother;
    jboolean       eq;
    if(opid != Py_EQ && opid != Py_NE) {
        /* TODO The doc says to raise type error. */
        Py_INCREF(Py_NotImplemented);
        return Py_NotImplemented;
    }
    selfType = PyJType_GetPyJType(self);
    if(selfType == NULL){
        PyErr_SetString(PyExc_TypeError, "Cannot compare non-java type.");
        return NULL;
    }
    otherType = PyJType_GetPyJType(self);
    if(otherType == NULL){
        PyErr_SetString(PyExc_TypeError, "Cannot compare non-java type.");
        return NULL;
    }
    env = pyembed_get_env();
    jself = selfType->pyjtp_pytoj(env, self);
    jother = otherType->pyjtp_pytoj(env, other);
    eq = (*env)->CallBooleanMethod(env, jself.l, jmethod_object_equals, jother);
    if((opid == Py_EQ && eq) || (opid == Py_NE && !eq)){
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

/*
 * PyType_GenericAlloc does not incref the refcount of the type if it is not a
 * heaptype. pyjtypes are not heap types but need to incref.
 */
PyObject* PyJObject2_GenericAlloc(PyTypeObject* self, Py_ssize_t nitems) {
    PyObject* result = PyType_GenericAlloc(self, nitems);
    if(Py_TYPE(self)->tp_is_gc((PyObject*) self)){
       Py_XINCREF(self);
    }
    return result;
}

/*
 *
 */
void PyJObject2_GenericDealloc(PyObject* self) {
    
}

static PyObject* pyjobject2_new(PyTypeObject *type, PyObject *args, PyObject *kwds){
    PyJObject2Object *self;
    JNIEnv        *env;
    jclass        clazz;
    jmethodID     constructor;
    jobject       jobj;
    self = (PyJObject2Object *)type->tp_alloc(type, 0);
    if (self != NULL) {
      env = pyembed_get_env();
      clazz = ((PyJTypeObject*) type)->pyjtp_class;
      if(clazz){
        constructor = (*env)->GetMethodID(env, clazz, "<init>", "()V");
        if((*env)->ExceptionCheck(env) == JNI_TRUE){
          PyErr_SetString(PyExc_RuntimeError,
                          "Cannot construct object.");
          return NULL;
        }
        jobj=(*env)->NewObject(env,clazz, constructor);
        self->object = (*env)->NewGlobalRef(env,jobj);
        return (PyObject *)self;
      }else{
        printf("Doomed\n");
        // TODO Free and return null;
      }
    }
    return (PyObject *)self;
}
