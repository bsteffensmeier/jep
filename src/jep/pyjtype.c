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
#include "pyjmethod2.h"
#include "primitives/pyjprimitives.h"
#include "pyembed.h"
#include "pyjconverter.h"
#include "util.h"

/* Look up during Init, save for later. */
jclass jclass_java_lang_Class = NULL;

/* Look up during Init, save for later. */
static jmethodID jmethod_class_getName;

static PyObject* pyjtype_class_cache = NULL;

/*
 * Extenders are stored in a static linked list.
 *
 * TODO a PyDict might be faster.
 */
typedef struct PyJTypeExtender_{
    const char* class_name;
    pyjtype_extend extend_method;
    struct PyJTypeExtender_* next;
} PyJTypeExtender;

/*
 * TODO extenders need to be exposed as a privatish field of jep module
 */
static PyJTypeExtender* extenderList = NULL;

void PyJType_Extend(const char * className, pyjtype_extend extend_method){
    PyJTypeExtender* node = PyMem_Malloc(sizeof(PyJTypeExtender));
    node->class_name = className;
    node->extend_method = extend_method;
    node->next = extenderList;
    extenderList = node;
}

static void extend_internal(PyJTypeObject* type){
   PyJTypeExtender* node;
   for(node = extenderList; node != NULL; node = node->next){
       if(strcmp(type->pyjtp_pytype.tp_name,node->class_name) == 0){
           node->extend_method(type);
       }
   }
}

/* 
 * Load global jclass and jmethod objects
 *
 * This function returns 0 if everything loads successfully, and nonzero if
 * anything fails to load. On errors this method sets a Python Exception and
 * does not clear the java exception.
 */
static int pyjtype_init_jni_globals(JNIEnv *env) {
    jclass java_lang_class = NULL;

    java_lang_class = (*env)->FindClass(env, "java/lang/Class");
    if(java_lang_class == NULL){
        PyErr_SetString(PyExc_RuntimeError, 
                        "Failed to load java class java.lang.Class");
        return -1;
    }

    jmethod_class_getName = (*env)->GetMethodID(env, java_lang_class, "getName",
                                                "()Ljava/lang/String;");
    if(jmethod_class_getName == NULL){
        PyErr_SetString(PyExc_RuntimeError, 
                        "Failed to load java method java.lang.Class.getName()");
        return -1;
    }
    jclass_java_lang_Class = (*env)->NewGlobalRef(env,java_lang_class);
    return 0;
}

/*
 * Load and initialize PyJType_Type and everything it needs to function.
 *
 * TODO provide uninitialize method
 *
 * Returns 0 on success, nonzero on error.
 */
int PyJType_Init(JNIEnv *env) {
    if(pyjtype_init_jni_globals(env) != 0){
        return -1;
    }
    if(_PyJObject2_Init(env) != 0){
        return -1;
    }
    if(_PyJMethod2_Init(env) != 0){
        return -1;
    }

    /* Initialize java.lang.Class*/
    PyJType_Type.pyjtp_pytype.tp_dict = PyDict_New();
    if(PyJType_Type.pyjtp_pytype.tp_dict == NULL){
        return -1;
    }
    PyJType_Type.pyjtp_class = jclass_java_lang_Class;
    if(PyJMethod2_Get_Methods(env, jclass_java_lang_Object, 
                               PyJType_Type.pyjtp_pytype.tp_dict) != 0){
        Py_DECREF(PyJType_Type.pyjtp_pytype.tp_dict);
        PyJType_Type.pyjtp_pytype.tp_dict = NULL;
        return -1;
    }
    if(PyJMethod2_Get_Methods(env, jclass_java_lang_Class,
                               PyJType_Type.pyjtp_pytype.tp_dict) != 0){
        Py_DECREF(PyJType_Type.pyjtp_pytype.tp_dict);
        PyJType_Type.pyjtp_pytype.tp_dict = NULL;
        return -1;
    }
    PyJType_Type.pyjtp_pytype.tp_base = &PyType_Type;
    if(PyType_Ready(&(PyJType_Type.pyjtp_pytype)) < 0){
        printf("Horrible failure\n");
        Py_DECREF(PyJType_Type.pyjtp_pytype.tp_dict);
        PyJType_Type.pyjtp_pytype.tp_dict = NULL;
        return -1;
    }
    /* This makes java.lang.Class a java.lang.Class           */
    /* Its a pretty gnarly hack but so far no side affects... */
    /* 
     * TODO while we are screwing with python internals, it would
     *  probably be possible to multiple inherit from PyJObject2
     */
    Py_TYPE(&PyJType_Type) = &(PyJType_Type.pyjtp_pytype);

    /* Cache things */
    pyjtype_class_cache = PyDict_New();
    if(pyjtype_class_cache == NULL){
        return -1;
    }
    if (PyDict_SetItemString(pyjtype_class_cache, "java.lang.Class",
                             (PyObject*) (&PyJType_Type)) < 0){
        Py_DECREF(pyjtype_class_cache);
        return -1;
    }
    return 0;
}

PyJTypeObject* PyJType_GetPyJType(PyObject* object){
    PyTypeObject* type;
    if(object == NULL){
        return NULL;
    }
    type = Py_TYPE(object);
    if(Py_TYPE(type) == &(PyJType_Type.pyjtp_pytype)){
        return (PyJTypeObject*) type;
    }
    return NULL;
}

PyObject* pyjtype_load(JNIEnv*, jclass, PyObject*);

PyObject* PyJType_jtopy(JNIEnv *env, jclass class) {
    jstring     jClassName  = NULL;
    const char* cClassName  = NULL;
    PyObject*   pyClassName = NULL;
    PyObject*   pyjtype     = NULL;
    if(!class) {
        PyErr_SetString(PyExc_RuntimeError, "Invalid java class object.");
        return NULL;
    }
    /* Get the class name as a py string. */
    jClassName = (*env)->CallObjectMethod(env, class, jmethod_class_getName);
    if(!jClassName){
        // TODO would this throw a java exception?
        PyErr_SetString(PyExc_RuntimeError, "Failed to get java class name.");
        return NULL;
    }
    cClassName = (*env)->GetStringUTFChars(env, jClassName, 0);
    if(!cClassName){
        // TODO would this throw a java exception?
        PyErr_SetString(PyExc_RuntimeError, "Failed to get java class name.");
        return NULL;
    }
    pyClassName = PyString_FromString(cClassName);
    if(!pyClassName){
        return NULL;
    }
    (*env)->ReleaseStringUTFChars(env, jClassName, cClassName);
    cClassName = NULL;
    (*env)->DeleteLocalRef(env, jClassName);
    jClassName = NULL;

    pyjtype = PyDict_GetItem(pyjtype_class_cache, pyClassName);
    if (pyjtype){
        if(Py_TYPE(pyjtype) != &(PyJType_Type.pyjtp_pytype)){
            /*
             * This occurs if someone inserts something else into the cache. May
             * want to consider disallowing insertions.
             */
            PyErr_SetString(PyExc_RuntimeError,
                            "Found an object in the pyjtype cache that is not a "
                            "pyjtype");
            return NULL;
        }else{
            jclass curClass = ((PyJTypeObject*) pyjtype)->pyjtp_class;
            if(!((*env)->IsSameObject(env, class, curClass))){
                /* 
                 * Another check for cache screwiness or multiple class loaders.
                 * To handle multiple class loaders could consider switching value
                 * to a list of pyjtypes.
                 */
                PyErr_SetString(PyExc_RuntimeError,
                                "class of pyjtype is not the requested class.");
                return NULL;
            }
        }
        Py_INCREF(pyjtype);
        return pyjtype;
    }
    pyjtype = pyjtype_load(env, class, pyClassName);
    if(pyjtype){
        if(PyDict_SetItem(pyjtype_class_cache, pyClassName, pyjtype) < 0){
            Py_DECREF(pyjtype);
            Py_DECREF(pyClassName);
            return NULL;
        }
    }
    return pyjtype;
}

PyObject* pyjtype_load(JNIEnv *env, jclass class, PyObject* pyClassName) {
    PyObject*      pyobj   = NULL;
    PyJTypeObject* pyjtype = NULL;
    jclass         jsuper  = NULL;
    PyJTypeObject* pysuper = NULL;
    pyobj = PyType_GenericNew(&(PyJType_Type.pyjtp_pytype), 
                                             NULL, NULL);
    if(pyobj == NULL){
        return NULL;
    }
    pyjtype = (PyJTypeObject*) pyobj;
    pyjtype->pyjtp_class = (*env)->NewGlobalRef(env,class);
    pyjtype->pyjtp_pytype.tp_flags = Py_TPFLAGS_DEFAULT;
    /* TODO I don't own this memory, but Im not sure I care. */
    pyjtype->pyjtp_pytype.tp_name = PyString_AsString(pyClassName);
    pyjtype->pyjtp_pytype.tp_doc = "Dynamically created type for a java class.";
    jsuper=(*env)->GetSuperclass(env, class);
    if(jsuper){
    	/* Only primitive classes should miss this.*/
        pysuper = (PyJTypeObject*) PyJType_jtopy(env, jsuper);
        (*env)->DeleteLocalRef(env, jsuper);
        if(pysuper == NULL){
            Py_DECREF(pyobj);
            return NULL;
        }
        pyjtype->pyjtp_pytype.tp_base = &(pysuper->pyjtp_pytype);
        pyjtype->pyjtp_jtopy = pysuper->pyjtp_jtopy;
        pyjtype->pyjtp_pytoj = pysuper->pyjtp_pytoj;
    }
    pyjtype->pyjtp_pytype.tp_dict = PyDict_New();
    if(pyjtype->pyjtp_pytype.tp_dict == NULL){
        Py_DECREF(pyobj);
        return NULL;
    }
    PyJMethod2_Get_Methods(env, class, pyjtype->pyjtp_pytype.tp_dict);
    extend_internal(pyjtype);
    PyType_Ready(&(pyjtype->pyjtp_pytype));
    return pyobj;
}

static PyObject* pyjtype_fromname_internal(JNIEnv* env, const char* cClassName, int primitive){
    PyObject*   pyClassName  = NULL;
    PyObject*   pyjtype      = NULL;
    int         classNameLen = 0;
    int         i            = 0;
    char*       jniClassName = NULL;
    jclass      class        = NULL;
    if(!cClassName) {
        PyErr_SetString(PyExc_RuntimeError, "Must specify class name.");
        return NULL;
    }
    pyClassName = PyString_FromString(cClassName);
    if(!pyClassName){
        return NULL;
    }
    pyjtype = PyDict_GetItem(pyjtype_class_cache, pyClassName);
    if (pyjtype){
        /* TODO do I need to check for cache screwiness. */
        Py_INCREF(pyjtype);
        return pyjtype;
    }
    classNameLen = strlen(cClassName);
    jniClassName = PyMem_Malloc(classNameLen);
    if(jniClassName == NULL){
        return NULL;
    }
    for(i = 0 ; i <= classNameLen ; i += 1){
       if(cClassName[i] == '.'){
           jniClassName[i] = '/';
       }else{
           jniClassName[i] = cClassName[i];
       }
    }
    if(primitive){
        if(strcmp(cClassName, "int") == 0){
            PyJTypeObject* arrayClass = (PyJTypeObject*) PyJType_FromName(env, "[I");
            // TODO cache this.
            jmethodID m = (*env)->GetMethodID(env, jclass_java_lang_Class, "getComponentType", "()Ljava/lang/Class;");
            class = (*env)->CallObjectMethod(env, arrayClass->pyjtp_class, m);
        }
        // TODO handle other primitives
    }else{
        class = (*env)->FindClass(env,jniClassName);
    }
    if(class == NULL){
        // TODO process exception.
       	return NULL;
    }
    PyMem_Free(jniClassName);
    pyjtype = pyjtype_load(env, class, pyClassName);
    if(pyjtype){
        if(PyDict_SetItem(pyjtype_class_cache, pyClassName, pyjtype) < 0){
            Py_DECREF(pyjtype);
            Py_DECREF(pyClassName);
            return NULL;
        }
    }
    return pyjtype;
}

PyObject* PyJType_PrimitiveFromName(JNIEnv* env, const char* cClassName){
    return pyjtype_fromname_internal(env, cClassName, 1);
}

PyObject* PyJType_FromName(JNIEnv* env, const char* cClassName){
    return pyjtype_fromname_internal(env, cClassName, 0);
}


static PyObject*
pyjtype_new(PyTypeObject* type, PyObject* args, PyObject* kwargs){
    PyErr_SetString(PyExc_TypeError, "Cannot create a new java Class from python.");
    return NULL;
}

/*
 * This is very similar to type_dealloc used by PyType_Type. type_dealloc is not
 * used instead because instances of PyJType_Type do not set Py_TPFLAGS_HEAPTYPE.
 */
static void pyjtype_dealloc(PyObject* object){
    PyJTypeObject* jtype = NULL;
    JNIEnv*        env   = NULL;

    /* 
     * Both of the following checks are probably unnecessary, but at this stage
     * of development error checking is very agressive.
     */
    if(Py_TYPE(object) != &(PyJType_Type.pyjtp_pytype)){
        PyErr_SetString(PyExc_TypeError,
                        "pyjtype_dealloc recieved an object that is not a pyjtype");
        return;
    }
    jtype = (PyJTypeObject*) object;
    if(jtype == &PyJType_Type){
        PyErr_SetString(PyExc_TypeError,
                        "Failed attempt to deallocate PyJType_Type");
        return;
    }
    printf("The pyjtype for %s has been deallocated,\n", 
           jtype->pyjtp_pytype.tp_name);
    PyObject_GC_UnTrack(object);
    PyObject_ClearWeakRefs(object);

    Py_XDECREF(jtype->pyjtp_pytype.tp_base);
    Py_XDECREF(jtype->pyjtp_pytype.tp_bases);
    Py_XDECREF(jtype->pyjtp_pytype.tp_mro);
    Py_XDECREF(jtype->pyjtp_pytype.tp_cache);
    Py_XDECREF(jtype->pyjtp_pytype.tp_subclasses);

    /* TODO figure out whats up with name and doc */

    env = pyembed_get_env();
    (*env)->DeleteGlobalRef(env, jtype->pyjtp_class);

    Py_TYPE(object)->tp_free(object);
}

/*
 * This is very similar to type_traverse used by PyType_Type. type_traverse is not
 * used instead because instances of PyJType_Type do not set Py_TPFLAGS_HEAPTYPE.
 */
static int pyjtype_traverse(PyObject *object, visitproc visit, void *arg){
    PyTypeObject* type = NULL;
    /* 
     * The following check is probably unnecessary, but at this stage of 
     * development error checking is very agressive.
     */
    if(Py_TYPE(object) != &(PyJType_Type.pyjtp_pytype)){
        PyErr_SetString(PyExc_TypeError,
                        "pyjtype_traverse recieved an object that is not a "
                        "pyjtype");
        return -1;
    }
    type = (PyTypeObject*) object;
    Py_VISIT(type->tp_dict);
    Py_VISIT(type->tp_cache);
    Py_VISIT(type->tp_mro);
    Py_VISIT(type->tp_bases);
    Py_VISIT(type->tp_base);
    return 0;
}

/*
 * This is very similar to type_clear used by PyType_Type. type_clear is not
 * used instead because instances of PyJType_Type do not set Py_TPFLAGS_HEAPTYPE.
 */
static int pyjtype_clear(PyObject *object){
    PyTypeObject* type = NULL;
    /* 
     * The following check is probably unnecessary, but at this stage of 
     * development error checking is very agressive.
     */
    if(Py_TYPE(object) != &(PyJType_Type.pyjtp_pytype)){
        PyErr_SetString(PyExc_TypeError,
                        "pyjtype_clear recieved an object that is not a pyjtype");
        return -1;
    }
    type = (PyTypeObject*) object;
    if (type->tp_dict){
        PyDict_Clear(type->tp_dict);
    }
    Py_CLEAR(type->tp_mro);
    return 0;
}

static int pyjtype_is_gc(PyObject *type){
    return type != (PyObject*) &PyJType_Type;
}

PyObject* pyjtype_jtopy(JNIEnv* env, jvalue jval){
    return PyJType_jtopy(env, jval.l);
}

jvalue pyjtype_pytoj(JNIEnv* env, PyObject* pyobj){
    // TODO type check
    PyJTypeObject* pyjtype = (PyJTypeObject*) pyobj;
    jvalue val;
    val.l = (*env)->NewLocalRef(env, pyjtype->pyjtp_class);
    return val;
}

static PyObject* pyjtype_getClassCache(PyObject* self, void *closure){
    if(self != (PyObject*) &PyJType_Type){
       PyErr_SetString(PyExc_AttributeError,
                       "Class cache is only accessibly from PyJType object and"
                       " not from instances of the type");
       return NULL;
    }
    Py_INCREF(pyjtype_class_cache);
    // TODO things get weird if java.lang.class is removed from the dict.
    return pyjtype_class_cache;
}

static PyGetSetDef pyjtype_getseters[] = {
    {"__classCache__", 
     pyjtype_getClassCache, NULL,
     "Internal cache of pyjtypes",
     NULL},
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(pyjtype_doc,
    "An extension of the type class that wraps the java.lang.Class object\n"
    "All class wrappers are instances of this class.");

PyJTypeObject PyJType_Type = {
    {
        PyObject_HEAD_INIT(0)
        0, /* always 0 for types */               /* ob_size */
        "java.lang.Class",                        /* tp_name */
        sizeof(PyJTypeObject),                    /* tp_basicsize */
        0, /* instances are fixed size */         /* tp_itemsize */
        pyjtype_dealloc,                          /* tp_dealloc */
        0, /* default will call str */            /* tp_print */
        0, /* deprecated */                       /* tp_getattr */
        0, /* deprecated */                       /* tp_setattr */
        0, /* tp_richcompare is used instead */   /* tp_compare */
        0, /* Unset, maybe should be */           /* tp_repr */
        0, /* not a number */                     /* tp_as_number */
        0, /* not a sequence */                   /* tp_as_sequence */
        0, /* not a mapping */                    /* tp_as_mapping */
        PyJObject2_GenericHash,                   /* tp_hash  */
        0, /* inherited from type */              /* tp_call */
        PyJObject2_GenericStr,                    /* tp_str */
        0, /* inherited from type */              /* tp_getattro */
        0, /* inherited from type */              /* tp_setattro */
        0, /* not a buffer */                     /* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,  /* tp_flags */
        pyjtype_doc,                              /* tp_doc */
        pyjtype_traverse,                         /* tp_traverse */
        pyjtype_clear,                            /* tp_clear */
        PyJObject2_GenericCompare,                /* tp_richcompare */
        0, /* inherited from type */              /* tp_weaklistoffset */
        0, /* not iterable */                     /* tp_iter */
        0, /* not iterable */                     /* tp_iternext */
        0, /* no new methods */                   /* tp_methods */
        0, /* no new members */                   /* tp_members */
        pyjtype_getseters,                        /* tp_getset */
        0, /* set in PyJType_Init */              /* tp_base */
        0, /* set in PyJType_Init */              /* tp_dict */
        0, /* not a descriptor */                 /* tp_descr_get */
        0, /* not a descriptor */                 /* tp_descr_set */
        0, /* inherited from type */              /* tp_dictoffset */
        0, /* inherited from type */               /* tp_init */
        0, /* unsure, might be uncalled? */       /* tp_alloc */
        pyjtype_new,                              /* tp_new */
        0, /* inherited from type */              /* tp_free */
        pyjtype_is_gc,                            /* tp_is_gc */
    },
    0, /* set in PyJType_Init */                  /* pyjtp_class */
    pyjtype_jtopy,                                /* pyjtp_jtopy */
    pyjtype_pytoj,                                /* pyjtp_pytoj */
};
