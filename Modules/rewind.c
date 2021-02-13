#include <string.h>
#include "Python.h"
#include "frameobject.h"
#include "rewind.h"

static FILE *rewindLog;
static char rewindActive = 0;
static char rewindTraceOn = 0;
static int lastLine = -1;
static PyObject *knownObjectIds;
static PyObject *rewindBaseDir;

inline int equalstr(PyObject *obj, char *string) {
    return PyObject_RichCompareBool(obj, PyUnicode_FromString(string), Py_EQ);
}

void Rewind_Activate(const wchar_t *filename) {
    // Calculate the name of the rewind log file.
    // Given something.py, will look like something.rewind.
    PyObject *filename_obj = PyUnicode_FromWideChar(filename, -1);
    Py_ssize_t len = PyUnicode_GetLength(filename_obj);
    PyObject *dot = PyUnicode_FromString(".");
    Py_ssize_t idx = PyUnicode_Find(filename_obj, dot, 0, len, 0);
    PyObject *prefix = PySequence_GetSlice(filename_obj, 0, idx);
    PyObject *logFileName = PyUnicode_Concat(prefix, PyUnicode_FromString(".rewind"));
    const char * logFileNameUTF8 = PyUnicode_AsUTF8(logFileName);
    rewindLog = fopen(logFileNameUTF8, "w");
    knownObjectIds = PySet_New(NULL);
    rewindActive = 1;

    const char *baseDir = getenv("REWIND_BASEDIR");
    if (baseDir != NULL) {
        rewindBaseDir = PyUnicode_FromString(baseDir);
        fprintf(rewindLog, "REWIND_BASEDIR(");
        PyObject_Print(rewindBaseDir, rewindLog, 0);
        fprintf(rewindLog, ")\n");
    }
}

void Rewind_Deactivate() {
    rewindActive = 0;
    fclose(rewindLog);
}

void Rewind_PrintStringTuple(FILE *file, PyObject *stringTuple) {
    Py_ssize_t length = PyTuple_GET_SIZE(stringTuple);
    fprintf(file, ", %lu", length);
    for (int i = 0; i < length; i++) {
        PyObject *item = PyTuple_GET_ITEM(stringTuple, i);
        fprintf(file, ", ");
        PyObject_Print(item, file, 0);
    }
}

int Rewind_ShouldTrace(PyCodeObject *code) {
    if (rewindBaseDir) {
        Py_ssize_t length = PyUnicode_GetLength(code->co_filename);
        return PyUnicode_Tailmatch(code->co_filename, rewindBaseDir, 0, length, 0);
    } else {
        return 1;
    }
}

/*
PUSH_FRAME(
    filename, func_name, 
    globals, 
    num_local_vars, *local_varnames, 
    num_cell_vars, *cell_varnames,
    num_free_vars, *free_varnames,
    *local_vars,
    *cell_vars,
    *free_vars
)
*/
void Rewind_PushFrame(PyFrameObject *frame) {
    if (!rewindActive) return;

    PyCodeObject *code = frame->f_code;
    // Check if we should trace this code
    rewindTraceOn = Rewind_ShouldTrace(code);
    if (!rewindTraceOn) return;

    // when constructor (__init__) is invoked, track the object being built
    if (equalstr(code->co_name, "__init__")) {
        Rewind_TrackObject(*frame->f_localsplus);
    }
    PyObject **valuestack = frame->f_valuestack;
    // track all arguments, cellvars, and freevars
    for (PyObject **p = frame->f_localsplus; p < valuestack; p++) {
        PyObject *obj = *p;
        if (obj != NULL) {
            Rewind_TrackObject(obj);
        }
    }

    lastLine = -1;
    fprintf(rewindLog, "PUSH_FRAME(");
    PyObject_Print(code->co_filename, rewindLog, 0);
    fprintf(rewindLog, ", ");
    PyObject_Print(code->co_name, rewindLog, 0);
    fprintf(rewindLog, ", %lu", (unsigned long)frame->f_globals);
    fprintf(rewindLog, ", %lu", (unsigned long)frame->f_localsplus);
    
    Rewind_PrintStringTuple(rewindLog, code->co_varnames);
    Rewind_PrintStringTuple(rewindLog, frame->f_code->co_cellvars);
    Rewind_PrintStringTuple(rewindLog, frame->f_code->co_freevars);
    
    // serialize all arguments, cellvars, and freevars
    PyObject **p = frame->f_localsplus;
    while (p < valuestack) {
        fprintf(rewindLog, ", ");
        PyObject *obj = *p;
        Rewind_serializeObject(rewindLog, obj);
        p++;
    }
    fprintf(rewindLog, ")");
    fprintf(rewindLog, "\n");

    // fprintf(rewindLog, "RECURSION_DEPTH(%d)\n", tstate->recursion_depth);
}

void Rewind_PopFrame(PyFrameObject *frame) {
    if (!rewindActive) return;

    if (rewindTraceOn) {
        PyCodeObject *code = frame->f_code;
        fprintf(rewindLog, "POP_FRAME(");
        PyObject_Print(code->co_filename, rewindLog, 0);
        fprintf(rewindLog, ", ");
        PyObject_Print(code->co_name, rewindLog, 0);
        fprintf(rewindLog, ")\n");
        // fprintf(rewindLog, "RECURSION_DEPTH(%d)\n", tstate->recursion_depth);
    }

    PyFrameObject *prevFrame = frame->f_back;
    if (!prevFrame) {
        rewindTraceOn = 0;
        return;
    }
    // Check if we should trace this code
    rewindTraceOn = Rewind_ShouldTrace(prevFrame->f_code);
}

void Rewind_StoreDeref(PyObject *cell, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(cell);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "STORE_DEREF(%lu, ", (unsigned long)cell);
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_ListAppend(PyListObject *list, PyObject *value) {
    if (!rewindTraceOn) return;

    if (list == NULL || value == NULL) {
        return;
    }
    Rewind_TrackObject((PyObject *)list);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "LIST_APPEND(%lu, ", (unsigned long)list);
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_ListInsert(PyListObject *list, Py_ssize_t index, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "LIST_INSERT(%lu, %lu", (unsigned long)list, index);
    fprintf(rewindLog, ", ");
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_ListExtendBegin(PyListObject *list) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    // fprintf(rewindLog, "LIST_EXTEND_BEGIN(%lu)\n", (unsigned long)list);
}

void Rewind_ListRemove(PyListObject *list, PyObject *item) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    Rewind_TrackObject(item);
    fprintf(rewindLog, "LIST_REMOVE(%lu, ", (unsigned long)list);
    Rewind_serializeObject(rewindLog, item);
    fprintf(rewindLog, ")\n");
}

void Rewind_ListPop(PyListObject *list, Py_ssize_t index) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    fprintf(rewindLog, "LIST_POP(%lu, %lu)\n", (unsigned long)list, index);
}

void Rewind_ListClear(PyListObject *list) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    fprintf(rewindLog, "LIST_CLEAR(%lu)\n", (unsigned long)list);
}

void Rewind_ListReverse(PyListObject *list) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    fprintf(rewindLog, "LIST_REVERSE(%lu)\n", (unsigned long)list);
}

void Rewind_ListSort(PyListObject *list) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    fprintf(rewindLog, "LIST_SORT(%lu", (unsigned long)list);
    for (int i = 0; i < Py_SIZE(list); ++i) {
        PyObject *item = list->ob_item[i];
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, item);
    }
    fprintf(rewindLog, ")\n");
}

void Rewind_ListStoreSubscript(PyListObject *list, PyObject* key, PyObject* value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    Rewind_TrackObject(value);
    if (Py_IS_TYPE(key, &PySlice_Type)) {
        PySliceObject *slice = (PySliceObject *)key;
        fprintf(rewindLog, "LIST_STORE_SUBSCRIPT_SLICE(%lu, ", (unsigned long)list);
        PyObject_Print(slice->start, rewindLog, 0);
        fprintf(rewindLog, ", ");
        PyObject_Print(slice->stop, rewindLog, 0);
        fprintf(rewindLog, ", ");
        PyObject_Print(slice->step, rewindLog, 0);
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, value);
        fprintf(rewindLog, ")\n");
    } else {
        fprintf(rewindLog, "LIST_STORE_SUBSCRIPT(%lu, ", (unsigned long)list);
        Rewind_serializeObject(rewindLog, key);
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, value);
        fprintf(rewindLog, ")\n");
    }
}

void Rewind_ListDeleteSubscript(PyListObject *list, PyObject *key) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    if (Py_IS_TYPE(key, &PySlice_Type)) {
        PySliceObject *slice = (PySliceObject *)key;
        fprintf(rewindLog, "LIST_DELETE_SUBSCRIPT_SLICE(%lu, ", (unsigned long)list);
        PyObject_Print(slice->start, rewindLog, 0);
        fprintf(rewindLog, ", ");
        PyObject_Print(slice->stop, rewindLog, 0);
        fprintf(rewindLog, ", ");
        PyObject_Print(slice->step, rewindLog, 0);
        fprintf(rewindLog, ")\n");
    } else {
        fprintf(rewindLog, "LIST_DELETE_SUBSCRIPT(%lu, ", (unsigned long)list);
        Rewind_serializeObject(rewindLog, key);
        fprintf(rewindLog, ")\n");
    }
}

void Rewind_DictStoreSubscript(PyDictObject *dict, PyObject *key, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "DICT_STORE_SUBSCRIPT(%lu, ", (unsigned long)dict);
    Rewind_serializeObject(rewindLog, key);
    fprintf(rewindLog, ", ");
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictDeleteSubscript(PyDictObject *dict, PyObject *item) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(item);
    fprintf(rewindLog, "DICT_DELETE_SUBSCRIPT(%lu, ", (unsigned long)dict);
    Rewind_serializeObject(rewindLog, item);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictUpdate(PyDictObject *dict, PyObject *otherDict) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(otherDict);
    fprintf(rewindLog, "DICT_UPDATE(%lu, ", (unsigned long)dict);
    Rewind_serializeObject(rewindLog, otherDict);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictClear(PyDictObject *dict) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    fprintf(rewindLog, "DICT_CLEAR(%lu)\n", (unsigned long)dict);
}

void Rewind_DictPop(PyDictObject *dict, PyObject *key) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    fprintf(rewindLog, "DICT_POP(%lu, ", (unsigned long)dict);
    Rewind_serializeObject(rewindLog, key);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictPopItem(PyDictObject *dict, PyObject *key) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    fprintf(rewindLog, "DICT_POP_ITEM(%lu, ", (unsigned long)dict);
    Rewind_serializeObject(rewindLog, key);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictSetDefault(PyDictObject *dict, PyObject *key, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "DICT_SET_DEFAULT(%lu, ", (unsigned long)dict);
    Rewind_serializeObject(rewindLog, key);
    fprintf(rewindLog, ", ");
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_SetAdd(PySetObject *set, PyObject *newItem) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)set);
    Rewind_TrackObject(newItem);
    fprintf(rewindLog, "SET_ADD(%lu, ", (unsigned long)set);
    Rewind_serializeObject(rewindLog, newItem);
    fprintf(rewindLog, ")\n");
}

void Rewind_SetDiscard(PySetObject *set, PyObject *item) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)set);
    Rewind_TrackObject(item);
    fprintf(rewindLog, "SET_DISCARD(%lu, ", (unsigned long)set);
    Rewind_serializeObject(rewindLog, item);
    fprintf(rewindLog, ")\n");
}

void Rewind_SetClear(PySetObject *set) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)set);
    fprintf(rewindLog, "SET_CLEAR(%lu)\n", (unsigned long)set);
}

void Rewind_SetPrintItems(PySetObject *set) {
    Py_ssize_t pos;
    PyObject *key;
    Py_hash_t hash;
    pos = 0;
    while (_PySet_NextEntry((PyObject *)set, &pos, &key, &hash)) {
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, key);
    }
}

void Rewind_SetUpdate(PySetObject *set) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)set);
    fprintf(rewindLog, "SET_UPDATE(%lu", (unsigned long)set);
    Rewind_SetPrintItems(set);
    fprintf(rewindLog, ")\n");
}

void Rewind_YieldValue(PyObject *retval) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(retval);
    fprintf(rewindLog, "YIELD_VALUE(");
    Rewind_serializeObject(rewindLog, retval);
    fprintf(rewindLog, ")\n");
}

void Rewind_StoreName(PyObject *ns, PyObject *name, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(ns);
    Rewind_TrackObject(value);
    Rewind_TrackObject(name);
    fprintf(rewindLog, "STORE_NAME(%lu, ", (unsigned long)ns);
    Rewind_serializeObject(rewindLog, name);
    fprintf(rewindLog, ", ");
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_StoreFast(int index, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(value);
    fprintf(rewindLog, "STORE_FAST(%d, ", index);
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_StoreGlobal(PyObject *ns, PyObject *name, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(ns);
    Rewind_TrackObject(value);
    Rewind_TrackObject(name);
    fprintf(rewindLog, "STORE_GLOBAL(%lu, ", (unsigned long)ns);
    Rewind_serializeObject(rewindLog, name);
    fprintf(rewindLog, ", ");
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_DeleteGlobal(PyObject *ns, PyObject *name) {
    if (!rewindTraceOn) return;

    fprintf(rewindLog, "DELETE_GLOBAL(%lu, ", (unsigned long)ns);
    PyObject_Print(name, rewindLog, 0);
    fprintf(rewindLog, ")\n");
}

void Rewind_ReturnValue(PyObject *retval) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(retval);
    fprintf(rewindLog, "RETURN_VALUE(");
    Rewind_serializeObject(rewindLog, retval);
    fprintf(rewindLog, ")\n");
}

void Rewind_SetAttr(PyObject *obj, PyObject *attr, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(obj);
    Rewind_TrackObject(attr);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "STORE_ATTR(%lu, ", (unsigned long)obj);
    Rewind_serializeObject(rewindLog, attr);
    fprintf(rewindLog, ", ");
    Rewind_serializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_StringInPlaceAdd(PyObject *left, PyObject *right, PyObject *result) {
    if (!rewindTraceOn) return;

    if (left == result || right == result) {
        Rewind_TrackObject(result);
        fprintf(rewindLog, "STRING_INPLACE_ADD_RESULT(%lu, ", (unsigned long)result);
        PyObject_Print(result, rewindLog, 0);
        fprintf(rewindLog, ")\n");
    }
}

void Rewind_TrackList(PyObject *obj) {
    PyListObject *list = (PyListObject *)obj;
    for (int i = 0; i < Py_SIZE(list); ++i) {
        PyObject *item = list->ob_item[i];
        Rewind_TrackObject(item);
    }
    fprintf(rewindLog, "NEW_LIST(%lu", (unsigned long)list);
    for (int i = 0; i < Py_SIZE(list); ++i) {
        PyObject *item = list->ob_item[i];
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, item);
    }
    fprintf(rewindLog, ")\n");
}

void Rewind_TrackDict(PyObject *dict) {
    PyObject *value;
    Py_ssize_t pos = 0;
    PyObject *key;
    Py_hash_t hash;
    while (_PyDict_Next(dict, &pos, &key, &value, &hash)) {
        Rewind_TrackObject(key);
        Rewind_TrackObject(value);
    }
    fprintf(rewindLog, "NEW_DICT(%lu", (unsigned long)dict);

    pos = 0;
    while (_PyDict_Next(dict, &pos, &key, &value, &hash)) {
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, key);
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, value);
    }
    fprintf(rewindLog, ")\n");
}

void Rewind_TrackSet(PyObject *set) {
    Py_ssize_t pos = 0;
    PyObject *key;
    Py_hash_t hash;
    while (_PySet_NextEntry(set, &pos, &key, &hash)) {
        Rewind_TrackObject(key);
    }
    fprintf(rewindLog, "NEW_SET(%lu", (unsigned long)set);    
    pos = 0;
    while (_PySet_NextEntry(set, &pos, &key, &hash)) {
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, key);
    }
    fprintf(rewindLog, ")\n");
}

void Rewind_TrackTuple(PyObject *tuple) {
    Py_ssize_t n = PyTuple_GET_SIZE(tuple);
    for (int i = 0; i < n; i++) {
        PyObject *item = PyTuple_GET_ITEM(tuple, i);
        Rewind_TrackObject(item);
    }
    fprintf(rewindLog, "NEW_TUPLE(%lu", (unsigned long)tuple);
    for (int i = 0; i < n; i++) {
        PyObject *item = PyTuple_GET_ITEM(tuple, i);
        fprintf(rewindLog, ", ");
        Rewind_serializeObject(rewindLog, item);
    }
    fprintf(rewindLog, ")\n");
}

void Rewind_TrackClassObject(PyObject *obj) {
    PyObject **dictP = _PyObject_GetDictPtr(obj);
    PyObject *dict = NULL;
    if (dictP != NULL) {
        dict = *dictP;
    }

    if (dict != NULL) {
        PyObject *value;
        Py_ssize_t pos = 0;
        PyObject *key;
        Py_hash_t hash;
        while (_PyDict_Next(dict, &pos, &key, &value, &hash)) {
            Rewind_TrackObject(key);
            Rewind_TrackObject(value);
        }
    }

    fprintf(rewindLog, "NEW_OBJECT(%lu, ", (unsigned long)obj);
    PyTypeObject *type = (PyTypeObject *)PyObject_Type(obj);
    fprintf(rewindLog, "\"%s\", ", type->tp_name);
    Rewind_serializeObject(rewindLog, (PyObject *)type);
    
    if (dict != NULL) {
        PyObject *value;
        Py_ssize_t pos = 0;
        PyObject *key;
        Py_hash_t hash;
        while (_PyDict_Next(dict, &pos, &key, &value, &hash)) {
            fprintf(rewindLog, ", ");
            Rewind_serializeObject(rewindLog, key);
            fprintf(rewindLog, ", ");
            Rewind_serializeObject(rewindLog, value);
        }
    }
    
    fprintf(rewindLog, ")\n");
}

void Rewind_TrackObject(PyObject *obj) {
    if (obj == NULL) {
        return;
    }
    //Rewind_FlushDeallocatedIds();
    if (Rewind_isSimpleType(obj)) {
        return;
    }

    PyObject *id = (PyObject *)PyLong_FromLong((long)obj);
    if (PySet_Contains(knownObjectIds, id)) {
        Py_DECREF(id);
        return;
    }

    if (Py_IS_TYPE(obj, &PyList_Type)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
    
        Rewind_TrackList(obj);
    } else if (Py_IS_TYPE(obj, &PyUnicode_Type)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);

        fprintf(rewindLog, "NEW_STRING(%lu, ", (unsigned long)obj);
        Py_INCREF(obj);
        PyObject_Print(obj, rewindLog, 0);
        fprintf(rewindLog, ")\n");
        Py_DECREF(obj);
    } else if (Py_IS_TYPE(obj, &PyDict_Type)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackDict(obj);
    } else if (Py_IS_TYPE(obj, &PySet_Type)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackSet(obj);
    } else if (Py_IS_TYPE(obj, &PyTuple_Type)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackTuple(obj);
    } else if (Py_IS_TYPE(obj, &PyModule_Type)) {
        fprintf(rewindLog, "NEW_MODULE(%lu)\n", (unsigned long)obj);
    } else {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackClassObject(obj);
    }
}

inline int Rewind_isSimpleType(PyObject *obj) {
    return (Py_IS_TYPE(obj, &_PyNone_Type) ||
        Py_IS_TYPE(obj, &PyLong_Type) || 
        Py_IS_TYPE(obj, &PyBool_Type) ||
        Py_IS_TYPE(obj, &PyFloat_Type));
}

void Rewind_serializeObject(FILE *file, PyObject *obj) {
    if (obj == NULL) {
        fprintf(file, "None");
        return;
    }
    if (Rewind_isSimpleType(obj)) {
        PyObject_Print(obj, file, 0);
    } else {
        fprintf(file, "*%lu", (unsigned long)obj);
    }
}

// void Rewind_TrackException(PyObject *exceptionType, PyObject *exception) {
//     PyObject *id = (PyObject *)PyLong_FromLong((long)exception);
//     if (PySet_Contains(knownObjectIds, id)) {
//         Py_DECREF(id);
//         return;
//     }

//     PySet_Add(knownObjectIds, id);
//     fprintf(rewindLog, "NEW_EXCEPTION(%lu, ", (unsigned long)exception);
//     PyTypeObject *type = (PyTypeObject *)exceptionType;
//     fprintf(rewindLog, "\"%s\", ", type->tp_name);
//     PyObject *repr = PyObject_Repr(exception);
//     if (repr == NULL) {
//         fprintf(rewindLog, "None");
//     } else {
//         PyObject_Print(repr, rewindLog, 0);
//     }
//     fprintf(rewindLog, ")\n");
// }

void Rewind_Exception(PyObject *exceptionType, PyObject *exception, PyThreadState *tstate) {
    if (!rewindTraceOn) return;

    fprintf(rewindLog, "EXCEPTION(%lu, ", (unsigned long)exception);
    fprintf(rewindLog, "\"%s\", ", ((PyTypeObject *)exceptionType)->tp_name);
    PyObject *repr = PyObject_Repr(exception);
    if (repr == NULL) {
        fprintf(rewindLog, "None");
    } else {
        PyObject_Print(repr, rewindLog, 0);
    }
    fprintf(rewindLog, ", %d)\n", tstate->recursion_depth);
}

void Rewind_Dealloc(PyObject *obj) {
    if (!rewindTraceOn) return;

    PyObject *id = PyLong_FromLong((long)obj);
    PySet_Discard(knownObjectIds, id);
}

void Rewind_Log(char *message) {
    if (!rewindTraceOn) return;

    fprintf(rewindLog, "-- %s", message);
}

void printObject(FILE *file, PyObject *obj) {
    PyObject *type = PyObject_Type(obj);
    if (type == (PyObject *)&PyUnicode_Type || 
        type == (PyObject *)&_PyNone_Type ||
        type == (PyObject *)&PyLong_Type || 
        type == (PyObject *)&PyBool_Type) {
        PyObject_Print(obj, file, 0);
    } else {
        PyObject *typeName = PyObject_GetAttr(type, PyUnicode_FromString("__name__"));
        if (PyObject_RichCompareBool(typeName, PyUnicode_FromString("builtin_function_or_method"), Py_EQ) ||
            PyObject_RichCompareBool(typeName, PyUnicode_FromString("function"), Py_EQ) ||
            PyObject_RichCompareBool(typeName, PyUnicode_FromString("method_descriptor"), Py_EQ)) {
            PyObject *methodName = PyObject_GetAttr(obj, PyUnicode_FromString("__qualname__"));
            fprintf(file, "<");
            PyObject_Print(typeName, file, Py_PRINT_RAW);
            fprintf(file, " ");
            PyObject_Print(methodName, file, Py_PRINT_RAW);
            fprintf(file, "()");
            fprintf(file, ">");
        } else {
            fprintf(file, "<object ");
            PyObject_Print(typeName, file, Py_PRINT_RAW);
            fprintf(file, "(%lu)>", (unsigned long)obj);
        }
    }
}

void printStack(FILE *file, PyObject **stack_pointer, int level) {
    fprintf(file, "-- Stack(%d): [", level);
    for (int i = 1; i <= level; i++) {
        PyObject *obj = stack_pointer[-i];
        if (i != 1) {
            fprintf(file, ", ");
        }
        if (obj) {
            printObject(file, obj);
        }
    }
    fprintf(file, "]\n");
}

void logOp(char *label, PyObject **stack_pointer, int level, PyFrameObject *frame, int oparg) {
    if (!rewindTraceOn) return;

    int lineNo = PyFrame_GetLineNumber(frame);
    // if (lineNo <= 0) {
    //     return;
    // }
    if (rewindLog) {
        if (lastLine != lineNo) {
            fprintf(rewindLog, "VISIT(%d)\n", lineNo);
        }
        lastLine = lineNo;
    }
    // printStack(rewindLog, stack_pointer, level);
    fprintf(rewindLog, "-- %s(%d) on #%d\n", label, oparg, lineNo);
}
