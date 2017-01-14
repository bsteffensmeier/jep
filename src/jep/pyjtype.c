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

jmethodID classIsInterface   = NULL;
jmethodID classGetInterfaces = NULL;
jmethodID classGetMethods    = NULL;
jmethodID classGetFields     = NULL;

int PyJType_Check(PyObject *obj)
{
    return PyObject_TypeCheck(obj, &PyJType_Type);
}


static int load_methods(JNIEnv *env, jclass objClz, PyObject* dict){
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
    return 0;
EXIT_ERROR:
    return 1;
}

static int load_fields(JNIEnv *env, jclass objClz, PyObject* dict){
    int i;

    if (!JNI_METHOD(classGetFields, env,  JCLASS_TYPE, "getFields",
                    "()[Ljava/lang/reflect/Field;")) {
        process_java_exception(env);
        return 1;
    }

    jobjectArray fieldArray = (jobjectArray) (*env)->CallObjectMethod(env,
                 objClz,
                 classGetFields);
    if (process_java_exception(env) || !fieldArray) {
        return 1;
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
    return 0;
}

int pyjtype_fill_dict(JNIEnv* env, jclass clazz, PyObject* dict){
    if(load_methods(env, clazz, dict)){
        return 1;
    }
    if(load_fields(env, clazz, dict)){
        return 1;
    }
    PyDict_SetItemString(dict, "__java_class__", pyjobject_new(env, clazz));
    return 0;
}

int init_class_cache(JNIEnv* env, PyObject* cache){
    PyObject* iterDict = NULL;

    PyDict_SetItemString(cache, "java.lang.Object", (PyObject*) &PyJObject_Type);
    PyDict_SetItemString(cache, "java.lang.Class", (PyObject*) &PyJClass_Type);

    if(pyjtype_fill_dict(env, JOBJECT_TYPE, PyJObject_Type.tp_dict)){
        return 1;
    }

    if(pyjtype_fill_dict(env, JCLASS_TYPE, PyJClass_Type.tp_dict)){
        return 1;
    }

    if(pyjtype_fill_dict(env, JNUMBER_TYPE, PyJNumber_Type.tp_dict)){
        return 1;
    }
    PyDict_SetItemString(cache, "java.lang.Number", (PyObject*) &PyJNumber_Type);

    if(pyjtype_fill_dict(env, JMAP_TYPE, PyJMap_Type.tp_dict)){
        return 1;
    }
    PyDict_SetItemString(cache, "java.util.Map", (PyObject*) &PyJMap_Type);
    
#if PY_MAJOR_VERSION >= 3
    iterDict = PyJIterator_Type.tp_dict;
#else
    iterDict = PyDict_New();
#endif
    if(pyjtype_fill_dict(env, JITERATOR_TYPE, iterDict)){
        return 1;
    }
#if PY_MAJOR_VERSION < 3
    PyDict_DelItemString(iterDict, "next");
    PyDict_Update(PyJIterator_Type.tp_dict, iterDict);
    Py_DECREF(iterDict);
#endif

    PyDict_SetItemString(cache, "java.util.Iterator", (PyObject*) &PyJIterator_Type);

    if(pyjtype_fill_dict(env, JITERABLE_TYPE, PyJIterable_Type.tp_dict)){
        return 1;
    }
    PyDict_SetItemString(cache, "java.util.Iterable", (PyObject*) &PyJIterable_Type);

    if(pyjtype_fill_dict(env, JCOLLECTION_TYPE, PyJCollection_Type.tp_dict)){
        return 1;
    }
    PyDict_SetItemString(cache, "java.util.Collection", (PyObject*) &PyJCollection_Type);

    if(pyjtype_fill_dict(env, JLIST_TYPE, PyJList_Type.tp_dict)){
        return 1;
    }
    PyDict_SetItemString(cache, "java.util.List", (PyObject*) &PyJList_Type);

    return 0;
}

PyObject* pyjtype_wrap_new(JNIEnv *env, jclass clazz, PyObject* typeName){
    jboolean     interface     = JNI_FALSE;
    jobjectArray interfaces    = NULL;
    jint         interfacesIdx = 0;
    jint         numBases      = 0;
    Py_ssize_t   baseIdx       = 0;
    PyObject*    bases         = NULL;
    PyObject*    dict          = NULL;
    PyObject*    type          = NULL;
    //printf("Wrapping %s\n", PyString_AsString(typeName));
    if (!JNI_METHOD(classIsInterface, env, JCLASS_TYPE, "isInterface", "()Z")) {
        process_java_exception(env);
        return NULL;
    }
    if (!JNI_METHOD(classGetInterfaces, env, JCLASS_TYPE, "getInterfaces", "()[Ljava/lang/Class;")) {
        process_java_exception(env);
        return NULL;
    }
    interface = (*env)->CallBooleanMethod(env, clazz, classIsInterface);
    if(process_java_exception(env)){
        return NULL;
    }
    interfaces = (jobjectArray) (*env) ->CallObjectMethod(env, clazz, classGetInterfaces);
    if(!interfaces && process_java_exception(env)){
        return NULL;
    }
    if(interface != JNI_TRUE){
        numBases += 1;
    }
    if(interfaces){
        numBases += (*env)->GetArrayLength(env, interfaces);
    }
    bases = PyTuple_New(numBases);
    if(!bases){
        return NULL;
    }
    if(interface != JNI_TRUE){ 
        jclass super = (*env)->GetSuperclass(env, clazz);
        PyObject* superType = (PyObject*) PyJType_Wrap(env, super);
        if(!superType){
            goto FINALLY;
        }
        PyTuple_SET_ITEM(bases, baseIdx, superType);
        baseIdx += 1;
    }
    while(baseIdx < numBases){
        jclass superI = (jclass) (*env)->GetObjectArrayElement(env, interfaces, interfacesIdx);
        interfacesIdx += 1;
        PyObject* superType = (PyObject*) PyJType_Wrap(env, superI);
        if(!superType){
            goto FINALLY;
        }
        PyTuple_SET_ITEM(bases, baseIdx, superType);
        baseIdx += 1; 
    }
    dict = PyDict_New();
    if(!dict){
        goto FINALLY;
    }
    if(pyjtype_fill_dict(env, clazz, dict)){
        goto FINALLY;
    }
#if PY_MAJOR_VERSION < 3
    if ((*env)->IsAssignableFrom(env, clazz, JITERATOR_TYPE)) {
        PyDict_DelItemString(dict, "next");
    }
#endif
    type = PyObject_CallFunctionObjArgs((PyObject*) &PyJType_Type, typeName, bases, dict, NULL);
FINALLY:
    Py_XDECREF(bases);
    Py_XDECREF(dict);
    return type;
}

PyTypeObject* PyJType_Wrap(JNIEnv *env, jclass clazz){
    jstring    className  = NULL;
    PyObject*  typeName   = NULL;
    PyObject*  typeObject = NULL;
    JepThread* jepThread  = pyembed_get_jepthread();
    if (jepThread->classNameToPyType == NULL) {
        jepThread->classNameToPyType = PyDict_New();
        if(jepThread->classNameToPyType == NULL){
            return NULL;
        }
        if (init_class_cache(env, jepThread->classNameToPyType)){
            Py_DECREF(jepThread->classNameToPyType);
            jepThread->classNameToPyType = NULL;
            return NULL;
        }
    }
    className  = (*env)->CallObjectMethod(env, clazz, JCLASS_GET_NAME);
    if (!className) {
        process_java_exception(env);
        return NULL;
    }
    typeName = jstring_To_PyObject(env, className);
    (*env)->DeleteLocalRef(env, className);
    if(!typeName){
        return NULL;
    }
    typeObject = PyDict_GetItem(jepThread->classNameToPyType, typeName);
    if(typeObject == NULL){
        typeObject = pyjtype_wrap_new(env, clazz, typeName);
        if(typeObject != NULL){
            PyDict_SetItem(jepThread->classNameToPyType, typeName, typeObject);
        }
    }else{
        Py_INCREF(typeObject);
    }
    Py_DECREF(typeName);
    return (PyTypeObject*) typeObject;
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
            int res = f(descr, (PyObject*) type, v);
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
