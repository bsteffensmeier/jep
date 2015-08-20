/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 c-style: "K&R" -*- */
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

#include "Python.h"
#include <jni.h>

#include "pyembed.h"
#include "pyjarray.h"
#include "pyjobject2.h"
#include "pyjtype.h"
#include "pyjconverter.h"

typedef struct {
    PyObject_HEAD
    long it_index;
    jarray array;
} PyJArrayIterObject;

PyTypeObject PyJArrayIter_Type;

static PyObject *pyjarray_iter(PyObject *seq) {
    JNIEnv*        env       = NULL;
    PyJTypeObject* selfType;
    PyJArrayIterObject *it;
    if(PyType_Ready(&PyJArrayIter_Type) < 0){
        return NULL;
    }
   // if(!pyjarray_check(seq)) {
   //     PyErr_BadInternalCall();
   //     return NULL;
   // }
    selfType = PyJType_GetPyJType(seq);
    if(selfType == NULL){
        PyErr_SetString(PyExc_TypeError, "Cannot iterate non-java type.");
        return NULL;
    }
    it = PyObject_New(PyJArrayIterObject, &PyJArrayIter_Type);
    if (it == NULL){
        return NULL;
    }
    env = pyembed_get_env();
    it->it_index = 0;
    it->array = selfType->pyjtp_pytoj(env, seq).l;
    return (PyObject *)it;
}

PyObject* pyjarray_iter_self(PyObject *self) {
    Py_INCREF(self);
    return self;
}

static PyObject *pyjarrayiter_next(PyObject *object) {
    JNIEnv*        env       = NULL;
    jarray array;
    PyObject *item;
    // TODO type checking
    PyJArrayIterObject* it = (PyJArrayIterObject*) object;
    array = it->array;
    if (array == NULL){
        return NULL;
    }
    env = pyembed_get_env();
    if (it->it_index < (*env)->GetArrayLength(env, array)) {
        jobject obj = (*env)->GetObjectArrayElement(env,array, it->it_index);
        item = PyJObject2_jtopy(env, obj);
        ++it->it_index;
        return item;
    }
    it->array = NULL;
    return NULL;
}

PyTypeObject PyJArrayIter_Type = {
PyObject_HEAD_INIT(0)
0, /* ob_size */
"pyjarrayiterator", /* tp_name */
sizeof(PyJArrayIterObject), /* tp_basicsize */
0, /* tp_itemsize */
/* methods */
0, /* tp_dealloc */
0, /* tp_print */
0, /* tp_getattr */
0, /* tp_setattr */
0, /* tp_compare */
0, /* tp_repr */
0, /* tp_as_number */
0, /* tp_as_sequence */
0, /* tp_as_mapping */
0, /* tp_hash */
0, /* tp_call */
0, /* tp_str */
0, /* tp_getattro */
0, /* tp_setattro */
0, /* tp_as_buffer */
Py_TPFLAGS_DEFAULT |
Py_TPFLAGS_HAVE_ITER, /* tp_flags */
0, /* tp_doc */
0, /* tp_traverse */
0, /* tp_clear */
0, /* tp_richcompare */
0, /* tp_weaklistoffset */
pyjarray_iter_self, /* tp_iter */
pyjarrayiter_next, /* tp_iternext */
0, /* tp_methods */
0, /* tp_members */
0, /* tp_getset */
0, /* tp_base */
0, /* tp_dict */
0, /* tp_descr_get */
0, /* tp_descr_set */
};

int pyjarray_extend(PyJTypeObject* type){
    type->pyjtp_pytype.tp_iter = pyjarray_iter;
    return 0;
}


void PyJArray_Init(void){
    PyJType_Extend("[Ljava.lang.String;", pyjarray_extend);
}
