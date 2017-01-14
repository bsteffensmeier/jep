/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 c-style: "K&R" -*- */
/*
   jep - Java Embedded Python

   Copyright (c) 2016 JEP AUTHORS.

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

int PyJType_Check(PyObject *obj)
{
    return PyObject_TypeCheck(obj, &PyJType_Type);
}

jclass PyJType_GetClass(PyObject *obj){
    PyTypeObject* type = (PyTypeObject*) obj;
    PyJObject* pyClass = (PyJObject*) PyDict_GetItemString(type->tp_dict, "__java_class__");
    //printf("GetClass %d %d %d %s %s\n", pyClass, pyClass->object, pyClass->clazz, type->tp_name, PyString_AsString(PyObject_Str(pyClass)));
    return (jclass) pyClass->object;
}

int pyjtype_setattro(PyTypeObject *type, PyObject *name, PyObject *v){
    PyObject *descr;

    descr = _PyType_Lookup(type, name);

     if (descr != NULL) {
        descrsetfunc f = descr->ob_type->tp_descr_set;
        if (f != NULL) {
            Py_INCREF(descr);
            int res = f(descr, type, v);
            Py_DECREF(descr);
            return res;
        }
    }

    PyErr_Format(PyExc_TypeError, "can't set non-field attributes of java wrapper type '%s'", type->tp_name);
    return -1;
}

PyTypeObject PyJType_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "jep.PyJType",
    0,
    0,
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
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    (setattrofunc) pyjtype_setattro,          /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                       /* tp_flags */
    "Type for Java wrapper classes",                                 /* tp_doc */
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
