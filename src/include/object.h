#ifndef CLOX_OBJECT_H
#define CLOX_OBJECT_H

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value) isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_NATIVE_CLASS(value) isObjType(value, OBJ_NATIVE_CLASS)
#define IS_NATIVE_INSTANCE(value) isObjType(value, OBJ_NATIVE_INSTANCE)

#define AS_BOUND_METHOD(value) ((ObjBoundMethod*) AS_OBJ(value))
#define AS_CLASS(value) ((ObjClass*) AS_OBJ(value))
#define AS_CLOSURE(value) ((ObjClosure*) AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjFunction*) AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance*) AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative*) AS_OBJ(value)))
#define AS_STRING(value) ((ObjString*) AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString*) AS_OBJ(value))->chars)
#define AS_NATIVE_CLASS(value) ((ObjNativeClass*) AS_OBJ(value))
#define AS_NATIVE_INSTANCE(value) ((ObjNativeInstance*) AS_OBJ(value))

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_UPVALUE,
    OBJ_NATIVE_CLASS,
    OBJ_NATIVE_INSTANCE
} ObjType;

struct Obj {
    ObjType type;
    bool isMarked;
    struct Obj* next;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef bool (*NativeFn)(int argCount, Value* args, Value* result);

typedef struct {
    Obj obj;
    NativeFn function;
    int arity;
} ObjNative;

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

// Method fn: data=C resource, return false=ok, return true=error
typedef bool (*NativeMethodFn)(void* data, int argCount, Value* args, Value* result);
typedef void* (*NativeConstructorFn)(int argCount, Value* args);
typedef void  (*NativeFinalizerFn)(void* data);

typedef struct { const char* name; NativeMethodFn fn; } NativeMethod;

typedef struct {
    Obj                  obj;         // GC header — MUST be first; init'd by defineNativeClass()
    const char*          name;
    NativeConstructorFn  constructor;
    NativeFinalizerFn    finalizer;   // may be NULL
    const NativeMethod*  methods;     // NULL-terminated array
    int                  arity;       // -1 = variadic
} ObjNativeClass;

typedef struct {
    Obj obj;
    ObjNativeClass* klass;
    void* data;
} ObjNativeInstance;


ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjClass* newClass(ObjString* name);
ObjInstance* newInstance(ObjClass* klass);
ObjNativeInstance* newNativeInstance(ObjNativeClass* klass, void* data);
ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction(void);
ObjNative* newNative(NativeFn function, int arity);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
