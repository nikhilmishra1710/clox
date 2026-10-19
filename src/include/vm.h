#ifndef CLOX_VM_H
#define CLOX_VM_H

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (int) (FRAMES_MAX * UINT8_MAX)
#define FRAMES_MAX 64
#define HANDLERS_MAX 64
#define STACK_MAX (int) (FRAMES_MAX * UINT8_MAX)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    int frameCount;
    Value* stackTop;
    uint8_t* catchIp;
} ExceptionHandler;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    ExceptionHandler handlers[HANDLERS_MAX];
    int handlerCount;
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    Table modules;
    ObjModule* currentModule;
    Value switchValue;
    ObjUpvalue* openUpvalues;
    size_t bytesAllocated;
    size_t nextGC;
    Obj* objects;
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
    ObjString* initString;
} VM;

extern VM vm;
typedef enum { INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

void initVM(void);
void freeVM(void);
void runtimeError(const char* format, ...);
InterpretResult interpret(const char* source);
void push(Value value);
Value pop(void);

#endif
