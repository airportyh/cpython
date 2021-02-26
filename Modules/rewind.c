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

void Rewind_TrackCodeObject(PyCodeObject *code) {
    PyObject *id = (PyObject *)PyLong_FromLong((long)code);
    if (PySet_Contains(knownObjectIds, id)) {
        return;
    }

    fprintf(rewindLog, "NEW_CODE(%lu, ", (unsigned long)code);
    PyObject_Print(code->co_filename, rewindLog, 0);
    fprintf(rewindLog, ", ");
    PyObject_Print(code->co_name, rewindLog, 0);
    fprintf(rewindLog, ", %d", code->co_firstlineno);
    Rewind_PrintStringTuple(rewindLog, code->co_varnames);
    Rewind_PrintStringTuple(rewindLog, code->co_cellvars);
    Rewind_PrintStringTuple(rewindLog, code->co_freevars);
    fprintf(rewindLog, ")\n");
    PySet_Add(knownObjectIds, id);
}

/*
PUSH_FRAME(
    code_id,
    globals,
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

    Rewind_TrackCodeObject(code);
    
    PyObject **valuestack = frame->f_valuestack;
    // track all arguments, cellvars, and freevars
    for (PyObject **p = frame->f_localsplus; p < valuestack; p++) {
        PyObject *obj = *p;
        if (obj != NULL) {
            Rewind_TrackObject(obj);
        }
    }

    lastLine = -1;
    fprintf(rewindLog, "PUSH_FRAME(%lu", (unsigned long)code);
    fprintf(rewindLog, ", %lu", (unsigned long)frame->f_globals);
    fprintf(rewindLog, ", %lu", (unsigned long)frame->f_localsplus);
    
    // serialize all arguments, cellvars, and freevars
    PyObject **p = frame->f_localsplus;
    while (p < valuestack) {
        fprintf(rewindLog, ", ");
        PyObject *obj = *p;
        Rewind_SerializeObject(rewindLog, obj);
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
        fprintf(rewindLog, "POP_FRAME(%lu)\n", (unsigned long)code);
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
    Rewind_SerializeObject(rewindLog, value);
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
    Rewind_SerializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_ListInsert(PyListObject *list, Py_ssize_t index, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)list);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "LIST_INSERT(%lu, %lu", (unsigned long)list, index);
    fprintf(rewindLog, ", ");
    Rewind_SerializeObject(rewindLog, value);
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
    Rewind_SerializeObject(rewindLog, item);
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
        Rewind_SerializeObject(rewindLog, item);
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
        Rewind_SerializeObject(rewindLog, value);
        fprintf(rewindLog, ")\n");
    } else {
        fprintf(rewindLog, "LIST_STORE_SUBSCRIPT(%lu, ", (unsigned long)list);
        Rewind_SerializeObject(rewindLog, key);
        fprintf(rewindLog, ", ");
        Rewind_SerializeObject(rewindLog, value);
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
        Rewind_SerializeObject(rewindLog, key);
        fprintf(rewindLog, ")\n");
    }
}

void Rewind_DictStoreSubscript(PyDictObject *dict, PyObject *key, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "DICT_STORE_SUBSCRIPT(%lu, ", (unsigned long)dict);
    Rewind_SerializeObject(rewindLog, key);
    fprintf(rewindLog, ", ");
    Rewind_SerializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictDeleteSubscript(PyDictObject *dict, PyObject *key) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    fprintf(rewindLog, "DICT_DELETE_SUBSCRIPT(%lu, ", (unsigned long)dict);
    Rewind_SerializeObject(rewindLog, key);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictReplace(PyDictObject *dict, PyObject *otherDict) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(otherDict);
    fprintf(rewindLog, "DICT_REPLACE(%lu, ", (unsigned long)dict);
    Rewind_SerializeObject(rewindLog, otherDict);
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
    Rewind_SerializeObject(rewindLog, key);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictPopItem(PyDictObject *dict, PyObject *key) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    fprintf(rewindLog, "DICT_POP_ITEM(%lu, ", (unsigned long)dict);
    Rewind_SerializeObject(rewindLog, key);
    fprintf(rewindLog, ")\n");
}

void Rewind_DictSetDefault(PyDictObject *dict, PyObject *key, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject((PyObject *)dict);
    Rewind_TrackObject(key);
    Rewind_TrackObject(value);
    fprintf(rewindLog, "DICT_SET_DEFAULT(%lu, ", (unsigned long)dict);
    Rewind_SerializeObject(rewindLog, key);
    fprintf(rewindLog, ", ");
    Rewind_SerializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_SetAdd(PySetObject *set, PyObject *newItem) {
    if (!rewindTraceOn) return;
    if ((PyObject *)set == knownObjectIds) {
        return;
    }

    Rewind_TrackObject((PyObject *)set);
    Rewind_TrackObject(newItem);
    fprintf(rewindLog, "SET_ADD(%lu, ", (unsigned long)set);
    Rewind_SerializeObject(rewindLog, newItem);
    fprintf(rewindLog, ")\n");
}

void Rewind_SetDiscard(PySetObject *set, PyObject *item) {
    if (!rewindTraceOn) return;
    if ((PyObject *)set == knownObjectIds) {
        return;
    }

    Rewind_TrackObject((PyObject *)set);
    Rewind_TrackObject(item);
    fprintf(rewindLog, "SET_DISCARD(%lu, ", (unsigned long)set);
    Rewind_SerializeObject(rewindLog, item);
    fprintf(rewindLog, ")\n");
}

void Rewind_SetClear(PySetObject *set) {
    if (!rewindTraceOn) return;
    if ((PyObject *)set == knownObjectIds) {
        return;
    }

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
        Rewind_SerializeObject(rewindLog, key);
    }
}

void Rewind_SetUpdate(PySetObject *set) {
    if (!rewindTraceOn) return;
    if ((PyObject *)set == knownObjectIds) {
        return;
    }

    Rewind_TrackObject((PyObject *)set);
    fprintf(rewindLog, "SET_UPDATE(%lu", (unsigned long)set);
    Rewind_SetPrintItems(set);
    fprintf(rewindLog, ")\n");
}

void Rewind_YieldValue(PyObject *retval) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(retval);
    fprintf(rewindLog, "YIELD_VALUE(");
    Rewind_SerializeObject(rewindLog, retval);
    fprintf(rewindLog, ")\n");
}

void Rewind_StoreFast(int index, PyObject *value) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(value);
    fprintf(rewindLog, "STORE_FAST(%d, ", index);
    Rewind_SerializeObject(rewindLog, value);
    fprintf(rewindLog, ")\n");
}

void Rewind_ReturnValue(PyObject *retval) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(retval);
    fprintf(rewindLog, "RETURN_VALUE(");
    Rewind_SerializeObject(rewindLog, retval);
    fprintf(rewindLog, ")\n");
}

void Rewind_ObjectAssocDict(PyObject *obj, PyObject *dict) {
    if (!rewindTraceOn) return;

    Rewind_TrackObject(obj);
    Rewind_TrackObject(dict);
    fprintf(rewindLog, "OBJECT_ASSOC_DICT(%lu, %lu)\n", (unsigned long)obj, (unsigned long)dict);
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

void Rewind_CallStart() {
    if (!rewindTraceOn) return;

    fprintf(rewindLog, "CALL_START()\n");
}

void Rewind_CallEnd() {
    if (!rewindTraceOn) return;

    fprintf(rewindLog, "CALL_END()\n");
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
        Rewind_SerializeObject(rewindLog, item);
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
        Rewind_SerializeObject(rewindLog, key);
        fprintf(rewindLog, ", ");
        Rewind_SerializeObject(rewindLog, value);
    }
    fprintf(rewindLog, ")\n");
}

void Rewind_TrackDerivedDict(PyObject *dict) {
    PyObject *value;
    Py_ssize_t pos = 0;
    PyObject *key;
    Py_hash_t hash;
    PyTypeObject *type = (PyTypeObject *)PyObject_Type(dict);
    while (_PyDict_Next(dict, &pos, &key, &value, &hash)) {
        Rewind_TrackObject(key);
        Rewind_TrackObject(value);
    }
    fprintf(rewindLog, "NEW_DERIVED_DICT('%s', %lu", type->tp_name, (unsigned long)dict);

    pos = 0;
    while (_PyDict_Next(dict, &pos, &key, &value, &hash)) {
        fprintf(rewindLog, ", ");
        Rewind_SerializeObject(rewindLog, key);
        fprintf(rewindLog, ", ");
        Rewind_SerializeObject(rewindLog, value);
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
        Rewind_SerializeObject(rewindLog, key);
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
        Rewind_SerializeObject(rewindLog, item);
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
    Rewind_SerializeObject(rewindLog, (PyObject *)type);
    
    if (dict != NULL) {
        fprintf(rewindLog, ", %lu", (unsigned long)dict);
    }
    
    fprintf(rewindLog, ")\n");
}

void Rewind_TrackObject(PyObject *obj) {
    if (obj == NULL) {
        return;
    }
    if (obj == knownObjectIds) {
        return;
    }
    
    if (Rewind_IsSimpleType(obj)) {
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
    } else if (PyDict_Check(obj)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackDerivedDict(obj);
    } else if (Py_IS_TYPE(obj, &PySet_Type) || Py_IS_TYPE(obj, &PyFrozenSet_Type)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackSet(obj);
    } else if (Py_IS_TYPE(obj, &PyTuple_Type)) {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackTuple(obj);
    } else if (Py_IS_TYPE(obj, &PyModule_Type)) {
        fprintf(rewindLog, "NEW_MODULE(%lu)\n", (unsigned long)obj);
    } else if (Py_IS_TYPE(obj, &PyCell_Type)) {
        fprintf(rewindLog, "NEW_CELL(%lu)\n", (unsigned long)obj);
    } else if (Py_IS_TYPE(obj, &PyCode_Type)) {
        // Ignore, don't track here
    } else {
        PySet_Add(knownObjectIds, id);
        Py_DECREF(id);
        Rewind_TrackClassObject(obj);
    }
}

inline int Rewind_IsSimpleType(PyObject *obj) {
    return (Py_IS_TYPE(obj, &_PyNone_Type) ||
        Py_IS_TYPE(obj, &PyLong_Type) || 
        Py_IS_TYPE(obj, &PyBool_Type) ||
        Py_IS_TYPE(obj, &PyFloat_Type));
}

void Rewind_SerializeObject(FILE *file, PyObject *obj) {
    if (obj == NULL) {
        fprintf(file, "None");
        return;
    }
    if (Rewind_IsSimpleType(obj)) {
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

    PyObject *repr = PyObject_Repr(exception);
    
    fprintf(rewindLog, "EXCEPTION(%lu, ", (unsigned long)exception);
    fprintf(rewindLog, "\"%s\", ", ((PyTypeObject *)exceptionType)->tp_name);
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

void Rewind_TypeDict(PyObject *dict) {
    if (!rewindTraceOn) return;

    fprintf(rewindLog, "TYPE_DICT(%lu)\n", (unsigned long)dict);
}

void Rewind_Log(char *message) {
    if (!rewindTraceOn) return;

    fprintf(rewindLog, "-- %s", message);
}

void Rewind_ObjectSetBoolSlot(PyObject *obj, PyMemberDef *l, char value) {

}

void Rewind_ObjectSetByteSlot(PyObject *obj, PyMemberDef *l, char value) {

}

void Rewind_ObjectSetUByteSlot(PyObject *obj, PyMemberDef *l, unsigned char value) {

}

void Rewind_ObjectSetShortSlot(PyObject *obj, PyMemberDef *l, short value) {

}

void Rewind_ObjectSetUShortSlot(PyObject *obj, PyMemberDef *l, unsigned short value) {

}

void Rewind_ObjectSetIntSlot(PyObject *obj, PyMemberDef *l, int value) {

}

void Rewind_ObjectSetUIntSlot(PyObject *obj, PyMemberDef *l, unsigned int value) {

}

void Rewind_ObjectSetLongSlot(PyObject *obj, PyMemberDef *l, long value) {

}

void Rewind_ObjectSetULongSlot(PyObject *obj, PyMemberDef *l, unsigned long value) {

}

void Rewind_ObjectSetPySizeSlot(PyObject *obj, PyMemberDef *l, Py_ssize_t value) {

}

void Rewind_ObjectSetFloatSlot(PyObject *obj, PyMemberDef *l, float value) {

}

void Rewind_ObjectSetDoubleSlot(PyObject *obj, PyMemberDef *l, double value) {

}

void Rewind_ObjectSetObjectSlot(PyObject *obj, PyMemberDef *l, PyObject *value) {

}

void Rewind_ObjectSetCharSlot(PyObject *obj, PyMemberDef *l, char value) {

}

void Rewind_ObjectSetLongLongSlot(PyObject *obj, PyMemberDef *l, long long value) {

}

void Rewind_ObjectSetULongLongSlot(PyObject *obj, PyMemberDef *l, unsigned long long value) {
    
}

void Rewind_PrintObject(FILE *file, PyObject *obj) {
    if (obj == NULL) {
        fprintf(file, "None");
        return;
    }
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

void Rewind_PrintStack(PyObject **stack_pointer, int level) {
    // if (!rewindTraceOn) return;
    
    // fprintf(rewindLog, "-- Stack(%d): [", level);
    // for (int i = 1; i <= level; i++) {
    //     PyObject *obj = stack_pointer[-i];
    //     if (i != 1) {
    //         fprintf(rewindLog, ", ");
    //     }
    //     Rewind_PrintObject(rewindLog, obj);
    // }
    // fprintf(rewindLog, "]\n");
}

void Rewind_Op(char *label, PyFrameObject *frame, int oparg, int numArgs, ...) {
    if (!rewindTraceOn) return;
    
    va_list varargs;
    va_start(varargs, numArgs);

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
    fprintf(rewindLog, "-- %s(oparg=%d", label, oparg);

    for (int i = 0; i < numArgs; i++) {
        fprintf(rewindLog, ", ");
        PyObject *arg = va_arg(varargs, PyObject *);
        Rewind_PrintObject(rewindLog, arg);
    }
    va_end(varargs);
    fprintf(rewindLog, ") on #%d\n", lineNo);
}
