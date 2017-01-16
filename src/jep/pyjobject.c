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
static jmethodID compareTo       = 0;

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
    if (!PyJClass_Type.tp_base) {
        PyJClass_Type.tp_base = &PyJObject_Type;
    }
    if (PyType_Ready(&PyJClass_Type) < 0) {
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
            PyTypeObject* type = PyJType_Wrap(env, objClz);
            pyjob = (PyJObject*) PyType_GenericAlloc(type, 0); 
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
    if(Py_TYPE(pyjob) == &PyJClass_Type){
        ((PyJClassObject*) pyjob)->constructor  = 0;
    }

    (*env)->DeleteLocalRef(env, objClz);

    if (pyjobject_init(env, pyjob)) {
        return (PyObject *) pyjob;
    }
    return NULL;
}


PyObject* pyjobject_new_class(JNIEnv *env, jclass clazz)
{
     return (PyObject*) PyJType_Wrap(env, clazz);
}


static int pyjobject_init(JNIEnv *env, PyJObject *pyjob)
{
    jstring           className   = NULL;
    PyObject         *pyClassName = NULL;
    PyObject         *pyAttrName  = NULL;

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

static PyObject* pyjobject_tp_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds){
    //PyObject *boundConstructor = NULL;
    //PyObject *result           = NULL;

    PyObject* pyClass = PyDict_GetItemString(subtype->tp_dict, "__java_class__");

    return PyObject_Call(pyClass, args, kwds);

/*    PyObject* constructor = PyDict_GetItemString(subtype->tp_dict, "__constructor__");
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
    return result;*/
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
