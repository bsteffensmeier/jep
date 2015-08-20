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

#include "pyjconverter.h"
#include "pyjtype.h"

static PyObject* converter_dict;

typedef struct {
    PyObject_HEAD
    const char* className;
    int score;
    pyjconvert converter;
} PyJConverterObject;

PyTypeObject PyJConverter_Type = {
    PyObject_HEAD_INIT(0)
    0,
    "PyJConverter",
    sizeof(PyJConverterObject),
    0,
    0,                                       /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash  */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                       /* tp_flags */
    "Converter python objects into jvalues",  /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    0,                                        /* tp_init */
    0,                                        /* tp_alloc */
    NULL,                                     /* tp_new */
};

int PyJConverter_Init(){
    if(PyType_Ready(&PyJConverter_Type) < 0){
        return 1;
    }
    converter_dict = PyDict_New();
    if(converter_dict == NULL){
        return 1;
    }
    return 0;
}

void PyJConverter_Register(const char* className, 
                           PyObject* pytype,
                           int score,
                           pyjconvert converter){
    PyJConverterObject* pyconverter = PyObject_New(PyJConverterObject, &PyJConverter_Type);
    pyconverter->className = className;
    pyconverter->score = score;
    pyconverter->converter = converter;
    PyObject* list = PyDict_GetItem(converter_dict, pytype);
    if(list == NULL){
        list = PyList_New(0);
        PyDict_SetItem(converter_dict, pytype, list);
    }
    if(PyList_Append(list, (PyObject*) pyconverter) < 0){
        printf("Bad\n");
    }
}

PyJConverterObject* pyjconverter_get(PyObject* pyjtype, PyObject* pyobj){
    PyObject* list = PyDict_GetItem(converter_dict, (PyObject*) Py_TYPE(pyobj));
    if(list == NULL){
        return NULL;
    }
    Py_ssize_t size = PyList_Size(list);
    Py_ssize_t i;
    const char * className = ((PyJTypeObject*) pyjtype)->pyjtp_pytype.tp_name;
    for(i = 0 ; i < size; i += 1){
        PyJConverterObject* pyconverter = (PyJConverterObject*) PyList_GetItem(list, i);
        if(pyconverter == NULL){
            /* TODO I don't know why this is here */
            return NULL;
        }
        if (strcmp(pyconverter->className, className) == 0){
            return pyconverter;
        }
    }
    return NULL;
}

jvalue PyJConverter_Convert(JNIEnv* env, PyObject* pyjtype, PyObject* pyobj){
    if(Py_TYPE(Py_TYPE(pyobj)) == (PyTypeObject*) &PyJType_Type){
      // TODO make sure the type of the obj extends the type.
      return ((PyJTypeObject*) Py_TYPE(pyobj))->pyjtp_pytoj(env, pyobj);
    }
    PyJConverterObject* converter = pyjconverter_get(pyjtype, pyobj);
    if(converter){
        return converter->converter(env, pyobj);
    } 
    PyErr_Format(PyExc_RuntimeError, "Cannot convert %s to %s.", Py_TYPE(pyobj)->tp_name, ((PyJTypeObject*) pyjtype)->pyjtp_pytype.tp_name);
    jvalue val = {.l = NULL};
    return val;
}

int PyJConverter_Score(PyObject* pyjtype, PyObject* pyobj){
    if(Py_TYPE(Py_TYPE(pyobj)) == (PyTypeObject*) &PyJType_Type){
      // TODO make sure the type of the obj extends the type.
      return 0;
    }
    PyJConverterObject* converter = pyjconverter_get(pyjtype, pyobj);
    if(converter){
        return converter->score;
    } 
    return -1;
}

