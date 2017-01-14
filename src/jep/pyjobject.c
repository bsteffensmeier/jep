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

// https://bugs.python.org/issue2897
#include "structmember.h"

static int pyjobject_init(JNIEnv *env, PyJObject*);
static void pyjobject_init_subtypes(void);
static int  subtypes_initialized = 0;


static jmethodID objectEquals    = 0;
static jmethodID objectHashCode  = 0;
static jmethodID classGetMethods = 0;
static jmethodID classGetFields  = 0;
static jmethodID compareTo       = 0;

static PyTypeObject *baseType = NULL;

/*
 * MSVC requires tp_base to be set at runtime instead of in
 * the type declaration. :/  Since we are building an
 * inheritance tree of types, we need to ensure that all the
 * tp_base are set for the subtypes before we possibly use
 * those subtypes.
 *
 * See https://docs.python.org/2/extending/newtypes.html
 *     https://docs.python.org/3/extending/newtypes.html
 */
static void pyjobject_init_subtypes(void)
{
    baseType = PyMem_Malloc(sizeof(PyTypeObject));
    memcpy(baseType, &PyJObject_Type, sizeof(PyTypeObject));
        
    if (!PyJType_Type.tp_base) {
        PyJType_Type.tp_base = &PyType_Type;
    }
    if (PyType_Ready(&PyJType_Type) < 0) {
        return;
    }

    // start at the top with object
    if (!Py_TYPE(&PyJObject_Type)) {
        Py_TYPE(&PyJObject_Type) = &PyJType_Type;
    }
    if (PyType_Ready(&PyJObject_Type) < 0) {
        return;
    }
    //printf("%s\n", Py_TYPE(&PyJObject_Type)->tp_name);
    // next do number
    if (!PyJNumber_Type.tp_base) {
        PyJNumber_Type.tp_base = &PyJObject_Type;
    }
    if (PyType_Ready(&PyJNumber_Type) < 0) {
        return;
    }

    // next do iterable
    //if (!PyJIterable_Type.tp_base) {
    //    PyJIterable_Type.tp_base = &PyJObject_Type;
    //}
    if (PyType_Ready(&PyJIterable_Type) < 0) {
        return;
    }
    if (PyType_Ready(&PyJIterator_Type) < 0) {
        return;
    }

    // next do collection
    if (!PyJCollection_Type.tp_base) {
        PyJCollection_Type.tp_base = &PyJIterable_Type;
    }
    if (PyType_Ready(&PyJCollection_Type) < 0) {
        return;
    }

    // next do list
    //if (!PyJList_Type.tp_base) {
    //    PyJList_Type.tp_base = &PyJCollection_Type;
    //}
    if (PyType_Ready(&PyJList_Type) < 0) {
        return;
    }

    // last do map
    //if (!PyJMap_Type.tp_base) {
    //    PyJMap_Type.tp_base = &PyJObject_Type;
    //}
    if (PyType_Ready(&PyJMap_Type) < 0) {
        return;
    }

    subtypes_initialized = 1;
}

static void load_methods(JNIEnv *env, jclass objClz, PyObject* dict){
    int i;

    if (!JNI_METHOD(classGetMethods, env, JCLASS_TYPE, "getMethods",
                    "()[Ljava/lang/reflect/Method;")) {
        process_java_exception(env);
        goto EXIT_ERROR;
    }
    jobjectArray methodArray = (jobjectArray) (*env)->CallObjectMethod(env, objClz, classGetMethods);
    if (process_java_exception(env) || !methodArray) {
        goto EXIT_ERROR;
    }

    // for each method, create a new pyjmethod object
    jint len = (*env)->GetArrayLength(env, methodArray);
    for (i = 0; i < len; i++) {
        PyJMethodObject *pymethod = NULL;
        jobject rmethod = NULL;

        rmethod = (*env)->GetObjectArrayElement(env, methodArray, i);

        // make new PyJMethodObject, linked to pyjob
        pymethod = PyJMethod_New(env, rmethod);

        if (!pymethod) {
            continue;
        }

        if (pymethod->pyMethodName && PyString_Check(pymethod->pyMethodName)) {
            PyObject* cached = PyDict_GetItem(dict, pymethod->pyMethodName);
            if (cached && PyJMethod_Check(cached)) {
                PyObject* multimethod = PyJMultiMethod_New((PyObject*) pymethod, cached);
                PyDict_SetItem(dict, pymethod->pyMethodName, multimethod);
            } else if (cached && PyJMultiMethod_Check(cached)) {
                PyJMultiMethod_Append(cached, (PyObject*) pymethod);
            }else{
                if (PyDict_SetItem(dict, pymethod->pyMethodName, (PyObject*) pymethod) != 0) {
                    printf("WARNING: couldn't add method");
                }
            }
        }

        Py_DECREF(pymethod);
        (*env)->DeleteLocalRef(env, rmethod);
    } // end of looping over available methods
EXIT_ERROR:
    return;
}

static void load_fields(JNIEnv *env, jclass objClz, PyObject* dict){
    int i;

    if (!JNI_METHOD(classGetFields, env,  JCLASS_TYPE, "getFields",
                    "()[Ljava/lang/reflect/Field;")) {
        process_java_exception(env);
        return;
    }

    jobjectArray fieldArray = (jobjectArray) (*env)->CallObjectMethod(env,
                 objClz,
                 classGetFields);
    if (process_java_exception(env) || !fieldArray) {
        return;
    }

    // for each field, create a pyjfield object and
    // add to the internal members list.
    int len = (*env)->GetArrayLength(env, fieldArray);
    for (i = 0; i < len; i++) {
        jobject          rfield   = NULL;
        PyJFieldObject *pyjfield = NULL;

        rfield = (*env)->GetObjectArrayElement(env,
                                               fieldArray,
                                               i);

        pyjfield = pyjfield_new(env, rfield);

        if (!pyjfield) {
            continue;
        }

        if (pyjfield->pyFieldName && PyString_Check(pyjfield->pyFieldName)) {
                PyDict_SetItem(dict, pyjfield->pyFieldName, (PyObject*) pyjfield);
        }

        Py_DECREF(pyjfield);
        (*env)->DeleteLocalRef(env, rfield);
    }
    (*env)->DeleteLocalRef(env, fieldArray);
}

static PyTypeObject* pyjobject_load_type(JNIEnv *env, jclass objClz){
    JepThread   *jepThread;
    jstring className;
    PyObject* pyClassName;

    className = (*env)->CallObjectMethod(env, objClz, JCLASS_GET_NAME);
    if (process_java_exception(env) || !className) {
        return NULL;
    }
    pyClassName = jstring_To_PyObject(env, className);
    jepThread = pyembed_get_jepthread();
    if (jepThread->classNameToPyType == NULL) {
        jepThread->classNameToPyType = PyDict_New();
    }
    PyTypeObject* type = (PyTypeObject*) PyDict_GetItem(jepThread->classNameToPyType, pyClassName);
    if(type == NULL){
        //printf("%s\n", PyString_AsString(pyClassName));
        if((*env)->IsSameObject(env, objClz, JOBJECT_TYPE)){
            PyDict_SetItem(jepThread->classNameToPyType, pyClassName, (PyObject*) &PyJObject_Type);
            load_methods(env, objClz, PyJObject_Type.tp_dict);
            load_fields(env, objClz, PyJObject_Type.tp_dict);
            PyDict_SetItemString(PyJObject_Type.tp_dict, "__java_class__", pyjobject_new(env, objClz));
            Py_DECREF(pyClassName);
            return &PyJObject_Type;
        }else if((*env)->IsSameObject(env, objClz, JNUMBER_TYPE)){
            PyDict_SetItem(jepThread->classNameToPyType, pyClassName, (PyObject*) &PyJNumber_Type);
            load_methods(env, objClz, PyJNumber_Type.tp_dict);
            load_fields(env, objClz, PyJNumber_Type.tp_dict);
            PyDict_SetItemString(PyJNumber_Type.tp_dict, "__java_class__", pyjobject_new(env, objClz));
            Py_DECREF(pyClassName);
            return &PyJNumber_Type;

        }
        jclass super = (*env)->GetSuperclass(env, objClz);
        
        PyObject* bases = NULL;
        //if ((*env)->IsAssignableFrom(env, objClz, JNUMBER_TYPE)) {
        //    bases = PyTuple_New(2);
        //    PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
        //    PyTuple_SetItem(bases, 1, &PyJNumber_Type);
        //}else 
        if ((*env)->IsAssignableFrom(env, objClz, JLIST_TYPE)) {
            bases = PyTuple_New(2);
            PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
            PyTuple_SetItem(bases, 1, &PyJList_Type);
        }else if ((*env)->IsAssignableFrom(env, objClz, JCOLLECTION_TYPE)) {
            bases = PyTuple_New(2);
            PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
            PyTuple_SetItem(bases, 1, &PyJCollection_Type);
        }else if ((*env)->IsAssignableFrom(env, objClz, JITERABLE_TYPE)) {
            bases = PyTuple_New(2);
            PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
            PyTuple_SetItem(bases, 1, &PyJIterable_Type);
        }else if ((*env)->IsAssignableFrom(env, objClz, JITERATOR_TYPE)) {
            bases = PyTuple_New(2);
            PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
            PyTuple_SetItem(bases, 1, &PyJIterator_Type);
        }else if ((*env)->IsAssignableFrom(env, objClz, JMAP_TYPE)) {
            bases = PyTuple_New(2);
            PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
            PyTuple_SetItem(bases, 1, &PyJMap_Type);
        }else{
            bases = PyTuple_New(1);
            PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
        }
        //PyTuple_SetItem(bases, 0, pyjobject_load_type(env, super));
        //PyTuple_SetItem(bases, 0, &PyJObject_Type);
        PyObject* dict = PyDict_New();
        load_methods(env, objClz, dict);
        load_fields(env, objClz, dict);
        if(!(*env)->IsSameObject(env, objClz, JCLASS_TYPE)){
            PyDict_SetItemString(dict, "__java_class__", pyjobject_new(env, objClz));
        }
        type = PyObject_CallFunctionObjArgs(&PyJType_Type, pyClassName, bases, dict, NULL);
        if(PyErr_Occurred()){
            PyErr_Print();
        }
        //printf("%s\n", PyString_AsString(PyObject_Str(PyObject_Type(type))));
        PyDict_SetItem(jepThread->classNameToPyType, pyClassName, (PyObject*) type);
        if((*env)->IsSameObject(env, objClz, JCLASS_TYPE)){
            PyDict_SetItemString(type->tp_dict, "__java_class__", pyjobject_new(env, objClz));
        }
        /*
        type = PyMem_Malloc(sizeof(PyTypeObject));
        memcpy(type, baseType, sizeof(PyTypeObject));
        jclass super = (*env)->GetSuperclass(env, objClz);
        type->tp_base = pyjobject_load_type(env, super);
        // TODO I don't own this memory, but Im not sure I care. 
        type->tp_name = PyString_AsString(pyClassName);
        type->tp_doc = "Dynamically created type for a java class.";
        type->tp_dict = PyDict_New();
        if(type->tp_dict == NULL){
            Py_DECREF(type);
            return NULL;
        }
        if(strcmp(PyString_AsString(pyClassName), "java.lang.Class") != 0){
            //PyDict_SetItemString(type->tp_dict, "__java_class__", pyjobject_new(env, objClz));
        }
        load_methods(env, objClz, type->tp_dict);
        load_fields(env, objClz, type->tp_dict);
        if (PyType_Ready(type) < 0) {
            return NULL;
        }
        PyDict_SetItem(jepThread->classNameToPyType, pyClassName, (PyObject*) type);
        if(strcmp(PyString_AsString(pyClassName), "java.lang.Class") == 0){
            //PyDict_SetItemString(type->tp_dict, "__java_class__", pyjobject_new(env, objClz));
        }
        */
    }
    Py_INCREF(type);
    Py_INCREF(type);
    Py_INCREF(type);
    Py_INCREF(type);
    Py_DECREF(pyClassName);
    //printf("refcnt %d %s \n", ((PyObject*) type)->ob_refcnt, type->tp_name);
    return type;
}


// called internally to make new PyJObject instances
PyObject* pyjobject_new(JNIEnv *env, jobject obj)
{
    PyJObject    *pyjob;
    jclass        objClz;
    int           jtype;

    if (!subtypes_initialized) {
        pyjobject_init_subtypes();
    }
    if (!obj) {
        PyErr_Format(PyExc_RuntimeError, "Invalid object.");
        return NULL;
    }

    objClz = (*env)->GetObjectClass(env, obj);

    /*
     * There exist situations where a Java method signature has a return
     * type of Object but actually returns a Class or array.  Also if you
     * call Jep.set(String, Object[]) it should be treated as an array, not
     * an object.  Hence this check here to build the optimal jep type in
     * the interpreter regardless of signature.
     */
    jtype = get_jtype(env, objClz);
    if (jtype == JARRAY_ID) {
        return pyjarray_new(env, obj);
    //} else if (jtype == JCLASS_ID) {
    //    return pyjobject_new_class(env, obj);
    } else {
        // check for some of our extensions to pyjobject
        /*if ((*env)->IsInstanceOf(env, obj, JITERABLE_TYPE)) {
            if ((*env)->IsInstanceOf(env, obj, JCOLLECTION_TYPE)) {
                if ((*env)->IsInstanceOf(env, obj, JLIST_TYPE)) {
                    pyjob = (PyJObject*) pyjlist_new();
                } else {
                    // a Collection we have less support for
                    pyjob = (PyJObject*) pyjcollection_new();
                }
            } else {
                // an Iterable we have less support for
                pyjob = (PyJObject*) pyjiterable_new();
            }
        } else if ((*env)->IsInstanceOf(env, obj, JMAP_TYPE)) {
            pyjob = (PyJObject*) pyjmap_new();
        } else if ((*env)->IsInstanceOf(env, obj, JITERATOR_TYPE)) {
            pyjob = (PyJObject*) pyjiterator_new();
        //} else if ((*env)->IsInstanceOf(env, obj, JNUMBER_TYPE)) {
        //    pyjob = (PyJObject*) pyjnumber_new(); */
//        } else {
            PyTypeObject* type = pyjobject_load_type(env, objClz);
            pyjob = PyType_GenericAlloc(type, 0); 
            //if(type == &PyJObject_Type){
            //    pyjob = PyObject_New(PyJObject, type);
            //}else{
            //    pyjob = PyObject_GC_New(PyJObject, type);
            //}
//        }
    }


    pyjob->object      = (*env)->NewGlobalRef(env, obj);
    pyjob->clazz       = (*env)->NewGlobalRef(env, objClz);
    pyjob->attr        = PyDict_New();
    pyjob->finishAttr  = 0;

    (*env)->DeleteLocalRef(env, objClz);

    if (pyjobject_init(env, pyjob)) {
        return (PyObject *) pyjob;
    }
    return NULL;
}


PyObject* pyjobject_new_class(JNIEnv *env, jclass clazz)
{
    PyJObject       *pyjob;
    PyJClassObject  *pyjclass;  // same object as pyjob, just casted

    if (!clazz) {
        PyErr_Format(PyExc_RuntimeError, "Invalid class object.");
        return NULL;
    }
 
    if(1){
        return (PyObject*) pyjobject_load_type(env, clazz);
    }


    if (!PyJClass_Type.tp_base) {
        PyJClass_Type.tp_base = &PyJObject_Type;
    }
    if (PyType_Ready(&PyJClass_Type) < 0) {
        return NULL;
    }

    pyjclass           = PyObject_NEW(PyJClassObject, &PyJClass_Type);
    pyjob              = (PyJObject*) pyjclass;
    pyjob->object      = NULL;
    pyjob->clazz       = (*env)->NewGlobalRef(env, clazz);
    pyjob->attr        = PyDict_New();
    pyjob->finishAttr  = 0;

    if (pyjclass_init(env, (PyObject *) pyjob)) {
        if (pyjobject_init(env, pyjob)) {
            return (PyObject *) pyjob;
        }
    }
    return NULL;
}


static int pyjobject_init(JNIEnv *env, PyJObject *pyjob)
{
    jobjectArray      methodArray = NULL;
    jobjectArray      fieldArray  = NULL;
    int               i, len = 0;

    jstring           className   = NULL;
    PyObject         *pyClassName = NULL;
    PyObject         *pyAttrName  = NULL;

    JepThread   *jepThread;
    PyObject    *cachedMethodList = NULL;

    if ((*env)->PushLocalFrame(env, JLOCAL_REFS) != 0) {
        process_java_exception(env);
        return 0;
    }
    // ------------------------------ call Class.getMethods()

    /*
     * attach attribute java_name to the pyjobject instance to assist with
     * understanding the type at runtime
     */
    className = (*env)->CallObjectMethod(env, pyjob->clazz, JCLASS_GET_NAME);
    if (process_java_exception(env) || !className) {
        goto EXIT_ERROR;
    }
    pyClassName = jstring_To_PyObject(env, className);
    pyAttrName = PyString_FromString("java_name");
    //if (PyObject_SetAttr((PyObject *) pyjob, pyAttrName, pyClassName) == -1) {
    //    goto EXIT_ERROR;
    //}
    pyjob->javaClassName = pyClassName;
    Py_DECREF(pyAttrName);
    // then, get methodid for getMethods()
    if (!JNI_METHOD(classGetMethods, env, JCLASS_TYPE, "getMethods",
                    "()[Ljava/lang/reflect/Method;")) {
        process_java_exception(env);
        goto EXIT_ERROR;
    }
    /*
     * Performance improvement.  The code below is very similar to previous
     * versions except methods are now cached in memory.
     *
     * Previously every time you instantiate a pyjobject, JEP would get the
     * complete list of methods, turn them into pyjmethods, and add them as
     * attributes to the pyjobject.
     *
     * Now JEP retains a python dictionary in memory with a key of the fully
     * qualified Java classname to a list of pyjmethods. Since the
     * Java methods will never change at runtime for a particular Class, this
     * is safe and drastically speeds up pyjobject instantiation by reducing
     * reflection calls. We continue to set and reuse the pyjmethods as
     * attributes on the pyjobject instance, but if pyjobject_getattr sees a
     * pyjmethod, it will put it inside a pymethod and return that, enabling
     * the reuse of the pyjmethod for this particular object instance.
     *
     * We have the GIL at this point, so we can safely assume we're
     * synchronized and multiple threads will not alter the dictionary at the
     * same time.
     */
    jepThread = pyembed_get_jepthread();
    if (jepThread == NULL) {
        goto EXIT_ERROR;
    }
    if (jepThread->fqnToPyJmethods == NULL) {
        PyObject *methodCache = PyDict_New();
        jepThread->fqnToPyJmethods = methodCache;
    }

    cachedMethodList = PyDict_GetItem(jepThread->fqnToPyJmethods, pyClassName);
    if (cachedMethodList == NULL) {
        PyObject *pyjMethodList = NULL;
        pyjMethodList = PyList_New(0);

        // - GetMethodID fails when you pass the clazz object, it expects
        //   a java.lang.Class jobject.
        // - if you CallObjectMethod with the langClass jclass object,
        //   it'll return an array of methods, but they're methods of the
        //   java.lang.reflect.Method class -- not ->object.
        //
        // so what i did here was find the methodid using langClass,
        // but then i call the method using clazz. methodIds for java
        // classes are shared....
        methodArray = (jobjectArray) (*env)->CallObjectMethod(env, pyjob->clazz,
                      classGetMethods);
        if (process_java_exception(env) || !methodArray) {
            goto EXIT_ERROR;
        }

        // for each method, create a new pyjmethod object
        // and add to the internal methods list.
        len = (*env)->GetArrayLength(env, methodArray);
        for (i = 0; i < len; i++) {
            PyJMethodObject *pymethod = NULL;
            jobject rmethod = NULL;

            rmethod = (*env)->GetObjectArrayElement(env, methodArray, i);

            // make new PyJMethodObject, linked to pyjob
            pymethod = PyJMethod_New(env, rmethod);

            if (!pymethod) {
                continue;
            }

            if (pymethod->pyMethodName && PyString_Check(pymethod->pyMethodName)) {
                int multi = 0;
                Py_ssize_t cacheLen = PyList_Size(pyjMethodList);
                int cacheIndex = 0;
                for (cacheIndex = 0; cacheIndex < cacheLen; cacheIndex += 1) {
                    PyObject* cached = PyList_GetItem(pyjMethodList, cacheIndex);
                    if (PyJMethod_Check(cached)) {
                        PyJMethodObject* cachedMethod = (PyJMethodObject*) cached;
                        if (PyObject_RichCompareBool(pymethod->pyMethodName, cachedMethod->pyMethodName,
                                                     Py_EQ)) {
                            PyObject* multimethod = PyJMultiMethod_New((PyObject*) pymethod, cached);
                            PyList_SetItem(pyjMethodList, cacheIndex, multimethod);
                            multi = 1;
                            break;
                        }
                    } else if (PyJMultiMethod_Check(cached)) {
                        PyObject* methodName = PyJMultiMethod_GetName(cached);
                        if (PyObject_RichCompareBool(pymethod->pyMethodName, methodName, Py_EQ)) {
                            Py_DECREF(methodName);
                            PyJMultiMethod_Append(cached, (PyObject*) pymethod);
                            multi = 1;
                            break;
                        } else {
                            Py_DECREF(methodName);
                        }
                    }
                }
                if (!multi) {
                    if (PyList_Append(pyjMethodList, (PyObject*) pymethod) != 0) {
                        printf("WARNING: couldn't add method");
                    }
                }
            }

            Py_DECREF(pymethod);
            (*env)->DeleteLocalRef(env, rmethod);
        } // end of looping over available methods
        PyDict_SetItem(jepThread->fqnToPyJmethods, pyClassName, pyjMethodList);
        cachedMethodList = pyjMethodList;
        Py_DECREF(pyjMethodList); // fqnToPyJmethods will hold the reference
        (*env)->DeleteLocalRef(env, methodArray);
    } // end of setting up cache for this Java Class
/*
    len = (int) PyList_Size(cachedMethodList);
    for (i = 0; i < len; i++) {
        PyObject* name   = NULL;
        PyObject* cached = PyList_GetItem(cachedMethodList, i);
        if (PyJMethod_Check(cached)) {
            PyJMethodObject* cachedMethod = (PyJMethodObject*) cached;
            name = cachedMethod->pyMethodName;
            Py_INCREF(name);
        } else if (PyJMultiMethod_Check(cached)) {
            name = PyJMultiMethod_GetName(cached);
        }
        if (name) {
            if (PyObject_SetAttr((PyObject *) pyjob, name, cached) == -1) {
                goto EXIT_ERROR;
            }
            Py_DECREF(name);
        }
    } // end of cached method optimizations
*/

    // ------------------------------ process fields
    if (!JNI_METHOD(classGetFields, env,  JCLASS_TYPE, "getFields",
                    "()[Ljava/lang/reflect/Field;")) {
        process_java_exception(env);
        goto EXIT_ERROR;
    }

    fieldArray = (jobjectArray) (*env)->CallObjectMethod(env,
                 pyjob->clazz,
                 classGetFields);
    if (process_java_exception(env) || !fieldArray) {
        goto EXIT_ERROR;
    }
/*
    // for each field, create a pyjfield object and
    // add to the internal members list.
    len = (*env)->GetArrayLength(env, fieldArray);
    for (i = 0; i < len; i++) {
        jobject          rfield   = NULL;
        PyJFieldObject *pyjfield = NULL;

        rfield = (*env)->GetObjectArrayElement(env,
                                               fieldArray,
                                               i);

        pyjfield = pyjfield_new(env, rfield, pyjob);

        if (!pyjfield) {
            continue;
        }

        if (pyjfield->pyFieldName && PyString_Check(pyjfield->pyFieldName)) {
            if (PyObject_SetAttr((PyObject *) pyjob,
                                 pyjfield->pyFieldName,
                                 (PyObject *) pyjfield) == -1) {
                goto EXIT_ERROR;
            }
        }

        Py_DECREF(pyjfield);
        (*env)->DeleteLocalRef(env, rfield);
    }
    (*env)->DeleteLocalRef(env, fieldArray);
*/
    // we've finished the object.
    pyjob->finishAttr = 1;
    (*env)->PopLocalFrame(env, NULL);
    return 1;


EXIT_ERROR:
    (*env)->PopLocalFrame(env, NULL);

    if (PyErr_Occurred()) { // java exceptions translated by this time
        if (pyjob) {
            pyjobject_dealloc(pyjob);
        }
    }

    return 0;
}


void pyjobject_dealloc(PyJObject *self)
{
#if USE_DEALLOC
    JNIEnv *env = pyembed_get_env();
    if (env) {
        if (self->object) {
            (*env)->DeleteGlobalRef(env, self->object);
        }
        if (self->clazz) {
            (*env)->DeleteGlobalRef(env, self->clazz);
        }
    }

    Py_CLEAR(self->attr);
    Py_CLEAR(self->javaClassName);

    //PyObject_GC_Del(self);
#endif
}


int pyjobject_check(PyObject *obj)
{
    if (PyObject_TypeCheck(obj, &PyJObject_Type)) {
        return 1;
    }
    return 0;
}

// call toString() on jobject. returns null on error.
// excpected to return new reference.
PyObject* pyjobject_str(PyJObject *self)
{
    PyObject   *pyres     = NULL;
    JNIEnv     *env;

    env   = pyembed_get_env();
    if (self->object) {
        pyres = jobject_topystring(env, self->object);
    } else {
        pyres = jobject_topystring(env, self->clazz);
    }

    if (process_java_exception(env)) {
        return NULL;
    }

    // python doesn't like Py_None here...
    if (pyres == NULL) {
        return Py_BuildValue("s", "");
    }

    return pyres;
}


static PyObject* pyjobject_richcompare(PyJObject *self,
                                       PyObject *_other,
                                       int opid)
{
    JNIEnv *env;

    if (PyType_IsSubtype(Py_TYPE(_other), &PyJObject_Type)) {
        PyJObject *other = (PyJObject *) _other;
        jboolean eq;

        jobject target, other_target;

        target = self->object;
        other_target = other->object;

        // lack of object indicates it's a pyjclass
        if (!target) {
            target = self->clazz;
        }
        if (!other_target) {
            other_target = other->clazz;
        }

        if (opid == Py_EQ && (self == other || target == other_target)) {
            Py_RETURN_TRUE;
        }

        env = pyembed_get_env();
        eq = JNI_FALSE;
        // skip calling Object.equals() if op is > or <
        if (opid != Py_GT && opid != Py_LT) {
            // get the methodid for Object.equals()
            if (!JNI_METHOD(objectEquals, env, JOBJECT_TYPE, "equals",
                            "(Ljava/lang/Object;)Z")) {
                process_java_exception(env);
                return NULL;
            }

            eq = (*env)->CallBooleanMethod(
                     env,
                     target,
                     objectEquals,
                     other_target);
        }

        if (process_java_exception(env)) {
            return NULL;
        }

        if (((eq == JNI_TRUE) && (opid == Py_EQ || opid == Py_LE || opid == Py_GE)) ||
                (eq == JNI_FALSE && opid == Py_NE)) {
            Py_RETURN_TRUE;
        } else if (opid == Py_EQ || opid == Py_NE) {
            Py_RETURN_FALSE;
        } else {
            /*
             * All Java objects have equals, but we must rely on Comparable for
             * the more advanced operators.  Java generics cannot actually
             * enforce the type of other in self.compareTo(other) at runtime,
             * but for simplicity let's assume if they got it to compile, the
             * two types can be compared. If the types aren't comparable to
             * one another, a ClassCastException will be thrown.
             *
             * In Python 2 we will allow the ClassCastException to halt the
             * comparison, because it will most likely return
             * NotImplemented in both directions and Python 2 will devolve to
             * comparing the pointer address.
             *
             * In Python 3 we will catch the ClassCastException and return
             * NotImplemented, because there's a chance the reverse comparison
             * of other.compareTo(self) will work.  If both directions return
             * NotImplemented (due to ClassCastException), Python 3 will
             * raise a TypeError.
             */
            jint result;
#if PY_MAJOR_VERSION >= 3
            jthrowable exc;
#endif

            if (!(*env)->IsInstanceOf(env, self->object, JCOMPARABLE_TYPE)) {
                char* jname = PyString_AsString(self->javaClassName);
                PyErr_Format(PyExc_TypeError, "Invalid comparison operation for Java type %s",
                             jname);
                return NULL;
            }

            if (!JNI_METHOD(compareTo, env, JCOMPARABLE_TYPE, "compareTo",
                            "(Ljava/lang/Object;)I")) {
                process_java_exception(env);
                return NULL;
            }

            result = (*env)->CallIntMethod(env, target, compareTo, other_target);
#if PY_MAJOR_VERSION >= 3
            exc = (*env)->ExceptionOccurred(env);
            if (exc != NULL) {
                if ((*env)->IsInstanceOf(env, exc, CLASSCAST_EXC_TYPE)) {
                    /*
                     * To properly meet the richcompare docs we detect
                     * ClassException and return NotImplemented, enabling
                     * Python to try the reverse operation of
                     * other.compareTo(self).  Unfortunately this only safely
                     * works in Python 3.
                     */
                    (*env)->ExceptionClear(env);
                    Py_INCREF(Py_NotImplemented);
                    return Py_NotImplemented;
                }
            }
#endif
            if (process_java_exception(env)) {
                return NULL;
            }

            if ((result == -1 && opid == Py_LT) || (result == -1 && opid == Py_LE) ||
                    (result == 1 && opid == Py_GT) || (result == 1 && opid == Py_GE)) {
                Py_RETURN_TRUE;
            } else {
                Py_RETURN_FALSE;
            }
        }
    }

    /*
     * Reaching this point means we are comparing a Java object to a Python
     * object.  You might think that's not allowed, but the python doc on
     * richcompare indicates that when encountering NotImplemented, allow the
     * reverse comparison in the hopes that that's implemented.  This works
     * suprisingly well because it enables Python comparison operations on
     * things such as pyjobject != Py_None or
     * assertSequenceEqual(pyjlist, pylist) where each list has the same
     * contents.  This saves us from having to worry about if the java object
     * is on the left side or the right side of the operator.
     *
     * In short, this is intentional to keep comparisons working well.
     */
    Py_INCREF(Py_NotImplemented);
    return Py_NotImplemented;
}


// get attribute 'name' for object.
// uses obj->attr dict for storage.
// returns new reference.
PyObject* pyjobject_getattro(PyObject *obj, PyObject *name)
{
    PyObject *ret = PyObject_GenericGetAttr(obj, name);
    if (ret == NULL) {
        return NULL;
    } else if (PyJMethod_Check(ret) || PyJMultiMethod_Check(ret)) {
        /*
         * TODO Should not bind non-static methods to pyjclass objects, but not
         * sure yet how to handle multimethods and static methods.
         */
#if PY_MAJOR_VERSION >= 3
        PyObject* wrapper = PyMethod_New(ret, (PyObject*) obj);
#else
        PyObject* wrapper = PyMethod_New(ret, (PyObject*) obj,
                                         (PyObject*) Py_TYPE(obj));
#endif
        Py_DECREF(ret);
        return wrapper;
    } else if (pyjfield_check(ret)) {
        //PyObject *resolved = pyjfield_get((PyJFieldObject *) ret);
        //Py_DECREF(ret);
        //return resolved;
    }
    return ret;
}



static long pyjobject_hash(PyJObject *self)
{
    JNIEnv *env = pyembed_get_env();
    int   hash = -1;

    if (!JNI_METHOD(objectHashCode, env, JOBJECT_TYPE, "hashCode", "()I")) {
        process_java_exception(env);
        return -1;
    }

    if (self->object) {
        hash = (*env)->CallIntMethod(env, self->object, objectHashCode);
    } else {
        hash = (*env)->CallIntMethod(env, self->clazz, objectHashCode);
    }
    if (process_java_exception(env)) {
        return -1;
    }

    /*
     * this seems odd but python expects -1 for error occurred and other
     * built-in types then return -2 if the actual hash is -1
     */
    if (hash == -1) {
        hash = -2;
    }

    return hash;
}

static jmethodID classGetConstructors = NULL;

static PyObject* pyjobject_init_constructors(PyJObject *pyc)
{
    JNIEnv       *env         = NULL;
    jobjectArray  initArray   = NULL;
    int           initLen     = 0;
    PyObject     *pycallable  = NULL;
    int           i           = 0;

    env = pyembed_get_env();
    if ((*env)->PushLocalFrame(env, JLOCAL_REFS) != 0) {
        process_java_exception(env);
        return NULL;
    }

    if (!JNI_METHOD(classGetConstructors, env, JCLASS_TYPE, "getConstructors",
                    "()[Ljava/lang/reflect/Constructor;")) {
        process_java_exception(env);
        goto EXIT_ERROR;
    }

    initArray = (jobjectArray) (*env)->CallObjectMethod(env, pyc->object,
                classGetConstructors);
    if (process_java_exception(env) || !initArray) {
        goto EXIT_ERROR;
    }

    initLen   = (*env)->GetArrayLength(env, initArray);

    for (i = 0; i < initLen; i++) {
        PyObject *pyjinit     = NULL;
        jobject   constructor = NULL;

        constructor = (*env)->GetObjectArrayElement(env, initArray, i);
        if (process_java_exception(env) || !constructor) {
            goto EXIT_ERROR;
        }
        pyjinit = PyJConstructor_New(env, constructor);
        if (pyjinit == NULL) {
            goto EXIT_ERROR;
        }
        (*env)->DeleteLocalRef(env, constructor);

        if (i == 0) {
            pycallable = pyjinit;
        } else if (i == 1) {
            PyObject* firstInit = pycallable;
            pycallable = PyJMultiMethod_New(firstInit, pyjinit);
            Py_DECREF(firstInit);
            Py_DECREF(pyjinit);
            if (pycallable == NULL) {
                goto EXIT_ERROR;
            }
        } else {
            if (PyJMultiMethod_Append(pycallable, pyjinit) == -1) {
                Py_DECREF(pyjinit);
                goto EXIT_ERROR;
            }
            Py_DECREF(pyjinit);
        }
    }
    (*env)->DeleteLocalRef(env, initArray);

    if (!pycallable) {
        PyErr_Format(PyExc_TypeError, "No public constructor");
    }

    (*env)->PopLocalFrame(env, NULL);
    return pycallable;

EXIT_ERROR:
    (*env)->PopLocalFrame(env, NULL);
    Py_CLEAR(pycallable);
    return NULL;
}

static PyObject* pyjobject_tp_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds){
    PyObject *boundConstructor = NULL;
    PyObject *result           = NULL;

    PyObject* pyClass = PyDict_GetItemString(subtype->tp_dict, "__java_class__");
    PyObject* constructor = PyDict_GetItemString(subtype->tp_dict, "__constructor__");
    if (constructor == NULL) {
        constructor = pyjobject_init_constructors((PyJObject*) pyClass);
        if (constructor == NULL) {
            return NULL;
        }
        PyDict_SetItemString(subtype->tp_dict, "__constructor__", constructor);
    }
#if PY_MAJOR_VERSION >= 3
    boundConstructor = PyMethod_New(constructor, pyClass);
#else
    boundConstructor = PyMethod_New(constructor, pyClass,
                                    (PyObject*) subtype);
#endif
    result = PyObject_Call(boundConstructor, args, kwds);
    Py_DECREF(boundConstructor);
    return result;
}

static struct PyMethodDef pyjobject_methods[] = {
    { NULL, NULL }
};

static PyMemberDef pyjobject_members[] = {
    {"__dict__", T_OBJECT, offsetof(PyJObject, attr), READONLY},
    {0}
};

PyTypeObject PyJObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "jep.PyJObject",                          /* tp_name */
    sizeof(PyJObject),                        /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor) pyjobject_dealloc,           /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_compare */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    (hashfunc) pyjobject_hash,                /* tp_hash  */
    0,                                        /* tp_call */
    (reprfunc) pyjobject_str,                 /* tp_str */
    PyObject_GenericGetAttr,                  /* tp_getattro */
    PyObject_GenericSetAttr,                  /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
    Py_TPFLAGS_BASETYPE,                      /* tp_flags */
    "jobject",                                /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    (richcmpfunc) pyjobject_richcompare,      /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    pyjobject_methods,                        /* tp_methods */
    pyjobject_members,                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    offsetof(PyJObject, attr),                /* tp_dictoffset */
    0,                                        /* tp_init */
    0,                                        /* tp_alloc */
    pyjobject_tp_new,                         /* tp_new */
};
