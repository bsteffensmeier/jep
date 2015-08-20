/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 c-style: "K&R" -*- */
/* 
   jep - Java Embedded Python

   Copyright (c) 2004 - 2011 Mike Johnson.

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

#include "pyjmethod2.h"
#include "primitives/pyjprimitives.h"
#include "pyjobject2.h"
#include "util.h"
#include "pyembed.h"
#include "pyjconverter.h"

/* Frequently used classes are saved globally */
static jclass jclass_java_lang_reflect_Method;

/* Frequently used methods are saved globally */
static jmethodID jmethod_class_getDeclaredMethods;
static jmethodID jmethod_method_getName;
static jmethodID jmethod_method_getModifiers;
static jmethodID jmethod_method_getParameterTypes;
static jmethodID jmethod_method_getReturnType;

/* This is defined in the The Java™ Virtual Machine Specification. */
static const int modifier_public = 1;

PyObject* pyjmethod2_from_name(JNIEnv*, PyObject*);

/* 
 * Load all the global jclass and jmethod objects
 *
 * This function returns 0 if everything loads successfully, and nonzero if
 * anything fails to load. On errors this method sets a Python Exception and
 * does not clear the java exception.
 */
int _PyJMethod2_Init(JNIEnv *env) {
    jclass java_lang_reflect_method = NULL;
    java_lang_reflect_method = (*env)->FindClass(env, "java/lang/reflect/Method");
    if(java_lang_reflect_method == NULL){
        PyErr_SetString(PyExc_RuntimeError,
                        "Failed to load java class java.lang.reflect.Method");
        return 1;
    }
    jmethod_class_getDeclaredMethods = (*env)->GetMethodID(env, jclass_java_lang_Class, "getDeclaredMethods",
                                                   "()[Ljava/lang/reflect/Method;");
    if(jmethod_class_getDeclaredMethods == NULL){
        PyErr_SetString(PyExc_RuntimeError,
                        "Failed to load java method java.lang.Class.getDeclaredMethods()");
        return 1;
    }
    jmethod_method_getName = (*env)->GetMethodID(env, java_lang_reflect_method, "getName",
                                                 "()Ljava/lang/String;");
    if(jmethod_method_getName == NULL){
        PyErr_SetString(PyExc_RuntimeError,
                        "Failed to load java method java.lang.reflect.Method.getName()");
        return 1;
    }
    jmethod_method_getModifiers = (*env)->GetMethodID(env, java_lang_reflect_method, "getModifiers",
                                                 "()I");
    if(jmethod_method_getModifiers == NULL){
        PyErr_SetString(PyExc_RuntimeError,
                        "Failed to load java method java.lang.reflect.Method.getModifiers()");
        return 1;
    }
    jmethod_method_getParameterTypes = (*env)->GetMethodID(env, java_lang_reflect_method, "getParameterTypes",
                                                           "()[Ljava/lang/Class;");
    if(jmethod_method_getParameterTypes == NULL){
        PyErr_SetString(PyExc_RuntimeError,
                        "Failed to load java method java.lang.reflect.Method.getParamterTypes()");
        return 1;
    }
    jmethod_method_getReturnType = (*env)->GetMethodID(env, java_lang_reflect_method, "getReturnType",
                                                           "()Ljava/lang/Class;");
    if(jmethod_method_getReturnType == NULL){
        PyErr_SetString(PyExc_RuntimeError,
                        "Failed to load java method java.lang.reflect.Method.getReturnType()");
        return 1;
    }
    jclass_java_lang_reflect_Method = (*env)->NewGlobalRef(env,java_lang_reflect_method);
    return 0;
}

/*
 * Create pyjmethods for every method defined in cls and add the methods to the supplied dict.
 *
 * returns 0 on success and nonzero on error. If the error occurs in JNI than a
 * python exception is set and the java exception is not cleared.
 */
int PyJMethod2_Get_Methods(JNIEnv* env, jclass cls, PyObject* dict){
    jobjectArray methodArray;
    int len;
    int i;

    methodArray = (jobjectArray) (*env)->CallObjectMethod(env, cls, jmethod_class_getDeclaredMethods);
    if((*env)->ExceptionCheck(env) == JNI_TRUE){
        PyErr_SetString(PyExc_RuntimeError, 
                        "Failed to get java methods.");
        return 1;
    }
    len = (*env)->GetArrayLength(env, methodArray);
    for (i = 0; i < len; i += 1) {
        jobject rmethod = NULL;
        jint modifiers;

        rmethod = (*env)->GetObjectArrayElement(env, methodArray, i);
        modifiers = (*env)->CallIntMethod(env, rmethod, jmethod_method_getModifiers);
        if((*env)->ExceptionCheck(env) == JNI_TRUE){
            PyErr_SetString(PyExc_RuntimeError,
                            "Failed to get java method modifiers.");
            return 1;
        }
        if(modifiers & modifier_public){
          PyObject* pystr;
          PyObject* pyjmethod;
          jstring jstr;
          const char *methodName;

          jstr = (jstring) (*env)->CallObjectMethod(env, rmethod, jmethod_method_getName);
          if((*env)->ExceptionCheck(env) == JNI_TRUE){
              PyErr_SetString(PyExc_RuntimeError,
                              "Failed to get java method name.");
              return 1;
          }
          methodName = (*env)->GetStringUTFChars(env, jstr, NULL);
          pystr = PyString_FromString(methodName);
          (*env)->ReleaseStringUTFChars(env, jstr, methodName);
          pyjmethod = PyDict_GetItem(dict, pystr);
          if(pyjmethod == NULL){
              if(PyErr_Occurred() != NULL){
                  return 1;
              }           
              pyjmethod = pyjmethod2_from_name(env, pystr);
              if (PyDict_SetItem(dict, pystr, pyjmethod) < 0){
                  return 1;
              }
          }
          jReflectMethodListNode* node = PyMem_Malloc(sizeof(jReflectMethodListNode));
          memset(node, '\0', sizeof(jReflectMethodListNode));
          node->reflect_method = (*env)->NewGlobalRef(env, rmethod);
          node->next = ((PyJMethod2_Object*) pyjmethod)->relfectMethodList;
          ((PyJMethod2_Object*) pyjmethod)->relfectMethodList = node;
          (*env)->DeleteLocalRef(env, rmethod);
          (*env)->DeleteLocalRef(env, jstr);
        }
    }
    return 0;
}

static int pyjmethod2_lazy_init(JNIEnv* env, PyJMethod2_Object* pyjmethod){
    jReflectMethodListNode* reflectNode;
    if (pyjmethod->initialized){
         return 0;
    }
    for(reflectNode = pyjmethod->relfectMethodList; reflectNode ; reflectNode = reflectNode->next){
        jarray parameterTypes;
        int parameterCount;
        jmethodID methodID;
        jclass returnType;
        parameterTypes = (*env)->CallObjectMethod(env, reflectNode->reflect_method, jmethod_method_getParameterTypes);
        if(parameterTypes == NULL){
            PyErr_SetString(PyExc_RuntimeError,
                            "Failed to get java method parameter types.");
            return 1;
        }
        parameterCount = (*env)->GetArrayLength(env, parameterTypes);
        if(parameterCount > 10){
            /* TODO This is really bad */
            parameterCount = 10;
        }
        PyJTypeObject** pyParameterTypes = PyMem_Malloc(sizeof(PyJTypeObject*)*parameterCount);
        int i;
        for(i = 0 ; i < parameterCount ; i += 1){
             jclass jc = (*env)->GetObjectArrayElement(env, parameterTypes, i);
             pyParameterTypes[i] = (PyJTypeObject*) PyJType_jtopy(env, jc);
             if(pyParameterTypes[i] == NULL){
                 PyErr_SetString(PyExc_RuntimeError,
                                "Failed to process java method parameter types.");
                 return 1;
             }
        }
        methodID = (*env)->FromReflectedMethod(env, reflectNode->reflect_method);
        jmethodListNode* node = PyMem_Malloc(sizeof(jmethodListNode));
        memset(node, '\0', sizeof(jmethodListNode));
        node->method = methodID;
        returnType = (*env)->CallObjectMethod(env, reflectNode->reflect_method, jmethod_method_getReturnType);
        if(returnType == NULL){
             PyErr_SetString(PyExc_RuntimeError,
                            "Failed to get java method return types.");
             return 1;
        }
        node->returnType = (PyJTypeObject*) PyJType_jtopy(env, returnType);
        node->parameterTypes = pyParameterTypes;
        node->next = pyjmethod->methodList[parameterCount];
        pyjmethod->methodList[parameterCount] = node;
    }
    pyjmethod->initialized = 1;
    return 0;
}

PyObject* pyjmethod2_from_name(JNIEnv *env, PyObject* name) {
    if(PyType_Ready(&(PyJMethod2_Type.pyjtp_pytype)) < 0){
        return NULL;
    }
    PyJMethod2_Object *methodObj = PyObject_New(PyJMethod2_Object, &(PyJMethod2_Type.pyjtp_pytype));
    methodObj->name = name;
    methodObj->initialized = 0;
    methodObj->relfectMethodList = 0;
    return (PyObject*) methodObj;
}

static PyObject* pyjmethod2_call_object(JNIEnv* env, jobject self, jmethodID method, jvalue* args){
	jobject result = (*env)->CallObjectMethodA(env, self, method, args);
    return PyJObject2_jtopy(env, result);
}

static PyObject* pyjmethod2_call_int(JNIEnv* env, jobject self, jmethodID method, jvalue* args){
	jint result = (*env)->CallIntMethodA(env, self, method, args);
	return PyJInt_jtopy(env, result);
}

static PyObject* pyjmethod2_call_boolean(JNIEnv* env, jobject self, jmethodID method, jvalue* args){
	jint result = (*env)->CallBooleanMethodA(env, self, method, args);
	return PyJBoolean_jtopy(env, result);
}

static PyObject* pyjmethod2_call(PyJMethod2_Object *self,
                                PyObject *args,
                                PyObject *keywords) {
    int           argsSize;
    JNIEnv        *env;
    jmethodListNode* node;
    PyObject*     arg;
    jobject jarg;
    jvalue* jargs;
    int i;
    PyObject* result;
    argsSize = PyTuple_Size(args);
    if(argsSize > 10){
        PyErr_SetString(PyExc_RuntimeError, "Provide less than 10 args to pyjmethod2 please");
        return NULL;
    }
    env = pyembed_get_env();
    pyjmethod2_lazy_init(env, self);
    node = self->methodList[argsSize - 1];
    if(node == NULL){
        PyErr_Format(PyExc_RuntimeError, "%s does not accept %d args.", PyString_AsString(self->name), argsSize - 1);
        return NULL;
    }
    jargs = (jvalue *) PyMem_Malloc(sizeof(jvalue) * (argsSize - 1));
    arg = PyTuple_GetItem(args, 0);
    jarg = PyJConverter_Convert(env, NULL, arg).l;
    if(jarg == NULL){
        return NULL;
    }
    for (i = 1; i < argsSize; i += 1) {
      jargs[i - 1] = PyJConverter_Convert(env, (PyObject*) node->parameterTypes[i - 1], PyTuple_GetItem(args, i));
      if(jargs[i - 1].l == NULL && PyErr_Occurred() != NULL){
          return NULL;
      }
    }
    // TODO support others, look up while initializing
    const char* returnType = node->returnType->pyjtp_pytype.tp_name;
    if(strcmp(returnType,"int") == 0){
        result = pyjmethod2_call_int(env, jarg, node->method, jargs);
    }else if(strcmp(returnType,"boolean") == 0){
        result = pyjmethod2_call_boolean(env, jarg, node->method, jargs);
    }else{
        result = pyjmethod2_call_object(env, jarg, node->method, jargs);
    }
    PyMem_Free(jargs);
    return result;
}

int pyjmethod2_descr_set(PyObject *self, PyObject *obj, PyObject *value){
    PyErr_SetString(PyExc_AttributeError, "Cannot set attribute that is a pyjmethod.");
    return -1;
}

static PyObject* pyjmethod2_descr_get(PyObject *func, PyObject *obj, PyObject *type){
    if (obj == Py_None){
        obj = NULL;
    }
    return PyMethod_New(func, obj, type);
}

static PyObject* pyjmethod2_get_name(PyJMethod2_Object *op){
    Py_INCREF(op->name);
    return op->name;
}

static PyGetSetDef pyjmethod2_getsetlist[] = {
    {"__name__", (getter)pyjmethod2_get_name, NULL},
    {NULL} /* Sentinel */
};

PyJTypeObject PyJMethod2_Type = {
    {
        PyObject_HEAD_INIT(0)
        0,                                        /* ob_size */
        "pyjmethod2",                             /* tp_name */
        sizeof(PyJMethod2_Object),                /* tp_basicsize */
        0,                                        /* tp_itemsize */
        0,                                        /* tp_dealloc */
        0,                                        /* tp_print */
        0,                                        /* tp_getattr */
        0,                                        /* tp_setattr */
        0,                                        /* tp_compare */
        0,                                        /* tp_repr */
        0,                                        /* tp_as_number */
        0,                                        /* tp_as_sequence */
        0,                                        /* tp_as_mapping */
        0,                                        /* tp_hash  */
        (ternaryfunc) pyjmethod2_call,            /* tp_call */
        0,                                        /* tp_str */
        0,                                        /* tp_getattro */
        0,                                        /* tp_setattro */
        0,                                        /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,                      /* tp_flags */
        "python type for a callable java method", /* tp_doc */
        0,                                        /* tp_traverse */
        0,                                        /* tp_clear */
        0,                                        /* tp_richcompare */
        0,                                        /* tp_weaklistoffset */
        0,                                        /* tp_iter */
        0,                                        /* tp_iternext */
        0,                                        /* tp_methods */
        0,                                        /* tp_members */
        pyjmethod2_getsetlist,                    /* tp_getset */
        0,                                        /* tp_base */
        0,                                        /* tp_dict */
        pyjmethod2_descr_get,                     /* tp_descr_get */
        pyjmethod2_descr_set,                     /* tp_descr_set */
        0,                                        /* tp_dictoffset */
        0,                                        /* tp_init */
        0,                                        /* tp_alloc */
        0,                                        /* tp_new */
        0,                                        /* tp_free */
    },
    NULL,
};

