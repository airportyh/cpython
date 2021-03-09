#include "Python.h"
#include "structmember.h"

void Rewind_Activate(const wchar_t * filename);

void Rewind_Deactivate(void);

int Rewind_IsSimpleType(PyObject *obj);

void Rewind_Cleanup(void);

void Rewind_PushFrame(PyFrameObject *frame);

void Rewind_PopFrame(PyFrameObject *frame);

void Rewind_StoreDeref(PyObject *cell, PyObject *value);

void Rewind_BuildList(PyObject *list);

void Rewind_ListAppend(PyListObject *list, PyObject *value);

void Rewind_ListInsert(PyListObject *list, Py_ssize_t index, PyObject *value);

void Rewind_ListExtendBegin(PyListObject *list);

void Rewind_ListExtendEnd(PyListObject *list);

void Rewind_ListRemove(PyListObject *list, PyObject *item);

void Rewind_ListPop(PyListObject *list, Py_ssize_t index);

void Rewind_ListClear(PyListObject *list);

void Rewind_ListReverse(PyListObject *list);

void Rewind_ListSort(PyListObject *list);

void Rewind_ListResizeAndShift(PyListObject *list, Py_ssize_t oldSize, Py_ssize_t size, Py_ssize_t index, Py_ssize_t numItems);

void Rewind_ListStoreIndex(PyListObject *list, size_t index, PyObject* value);

void Rewind_ListAssSliceStart(PyListObject *list);

void Rewind_ListAssSliceEnd(PyListObject *list);

void Rewind_ListDeleteIndex(PyListObject *list, size_t index);

void Rewind_ListStoreSubscript(PyListObject *list, PyObject* item, PyObject* value);

void Rewind_ListStoreItem(PyListObject *list, Py_ssize_t index, PyObject* value);

void Rewind_ListDeleteSubscript(PyListObject *list, PyObject *item);

void Rewind_DictStoreSubscript(PyDictObject *dict, PyObject *key, PyObject *value);

void Rewind_DictDeleteSubscript(PyDictObject *dict, PyObject *item);

void Rewind_DictReplace(PyDictObject *dict, PyObject *otherDict);

void Rewind_DictClear(PyDictObject *dict);

void Rewind_DictPop(PyDictObject *dict, PyObject *key);

void Rewind_DictPopItem(PyDictObject *dict, PyObject *key);

void Rewind_DictSetDefault(PyDictObject *dict, PyObject *key, PyObject *value);

void Rewind_SetAdd(PySetObject *set, PyObject *newItem);

void Rewind_SetDiscard(PySetObject *set, PyObject *item);

void Rewind_SetClear(PySetObject *set);

void Rewind_SetUpdate(PySetObject *set);

void Rewind_YieldValue(PyObject *retval);

void Rewind_StoreFast(int index, PyObject *value);

void Rewind_ReturnValue(PyObject *retval);

void Rewind_StringInPlaceAdd(PyObject *left, PyObject *right, PyObject *result);

void Rewind_CallStart(void);

void Rewind_CallEnd(void);

void Rewind_ObjectAssocDict(PyObject *obj, PyObject *dict);

void Rewind_Exception(PyObject *exceptionType, PyObject *exception, PyThreadState *tstate);

void Rewind_Dealloc(PyObject *obj);

void Rewind_TypeDict(PyObject *dict);

void Rewind_FlushDeallocatedIds(void);

void Rewind_TrackObject(PyObject *obj);

void Rewind_SerializeObject(FILE *file, PyObject *obj);

void Rewind_Log(char *message);

void Rewind_PrintObject(FILE *file, PyObject *obj);

void Rewind_ObjectSetBoolSlot(PyObject *obj, PyMemberDef *l, char value);

void Rewind_ObjectSetByteSlot(PyObject *obj, PyMemberDef *l, char value);

void Rewind_ObjectSetUByteSlot(PyObject *obj, PyMemberDef *l, unsigned char value);

void Rewind_ObjectSetShortSlot(PyObject *obj, PyMemberDef *l, short value);

void Rewind_ObjectSetUShortSlot(PyObject *obj, PyMemberDef *l, unsigned short value);

void Rewind_ObjectSetIntSlot(PyObject *obj, PyMemberDef *l, int value);

void Rewind_ObjectSetUIntSlot(PyObject *obj, PyMemberDef *l, unsigned int value);

void Rewind_ObjectSetLongSlot(PyObject *obj, PyMemberDef *l, long value);

void Rewind_ObjectSetULongSlot(PyObject *obj, PyMemberDef *l, unsigned long value);

void Rewind_ObjectSetPySizeSlot(PyObject *obj, PyMemberDef *l, Py_ssize_t value);

void Rewind_ObjectSetFloatSlot(PyObject *obj, PyMemberDef *l, float value);

void Rewind_ObjectSetDoubleSlot(PyObject *obj, PyMemberDef *l, double value);

void Rewind_ObjectSetObjectSlot(PyObject *obj, PyMemberDef *l, PyObject *value);

void Rewind_ObjectSetCharSlot(PyObject *obj, PyMemberDef *l, char value);

void Rewind_ObjectSetLongLongSlot(PyObject *obj, PyMemberDef *l, long long value);

void Rewind_ObjectSetULongLongSlot(PyObject *obj, PyMemberDef *l, unsigned long long value);

void Rewind_PrintStack(PyObject **stack_pointer, int level);

void Rewind_Op(char *label, PyFrameObject *frame, int oparg, int numArgs, ...);