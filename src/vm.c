#include "include/vm.h"
#include "include/chunk.h"
#include "include/compiler.h"
#include "include/debug.h" // IWYU pragma: keep
#include "include/memory.h"
#include "include/nativeFunction.h"
#include "include/object.h"
#include "include/table.h"
#include "include/value.h"

VM vm;

static void resetStack(void) {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.handlerCount = 0;
    vm.openUpvalues = NULL;
}

static char* getType(Value value) {
    if (IS_NUMBER(value))
        return "number";
    if (IS_BOOL(value))
        return "boolean";
    if (IS_NIL(value))
        return "nil";
    if (IS_OBJ(value)) {
        switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            return "string";
        // add more as needed
        case OBJ_NATIVE:
        case OBJ_FUNCTION:
            return "function";
        case OBJ_CLOSURE:
            return "closure";
        case OBJ_UPVALUE:
            return "upvalue";
        case OBJ_CLASS:
            return "class";
        case OBJ_INSTANCE:
            return "instance";
        case OBJ_BOUND_METHOD:
            return "method";
        case OBJ_NATIVE_CLASS:
            return "native class";
        case OBJ_NATIVE_INSTANCE:
            return "native instance";
        case OBJ_LIST:
            return "list";
        case OBJ_MAP:
            return "map";
        case OBJ_MODULE:
            return "module";
        }
    }
    return "unknown";
}

void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t instruction = (size_t) (frame->ip - function->chunk.code - 1);
        fprintf(stderr, "[line %d] in ", getLine(&function->chunk, (int) instruction));
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char* name, NativeFn function, int arity) {
    push(OBJ_VAL(copyString(name, (int) strlen(name))));
    push(OBJ_VAL(newNative(function, arity)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

static void defineNativeClass(ObjNativeClass* klass) {
    klass->obj.type = OBJ_NATIVE_CLASS;
    klass->obj.isMarked = true; // always marked — GC never frees static memory
    klass->obj.next = NULL;
    push(OBJ_VAL(copyString(klass->name, (int) strlen(klass->name))));
    push(OBJ_VAL((Obj*) klass));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM(void) {
    resetStack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
    initTable(&vm.modules);
    vm.currentModule = NULL;
    vm.initString = NULL;
    vm.initString = copyString("init", 4);

    defineNative("clock", clockNative, 0);
    defineNative("input", inputNative, -1);
    defineNativeClass(&fileClass);
}

void freeVM(void) {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeTable(&vm.modules);
    vm.initString = NULL;
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }
    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
            vm.stackTop[-argCount - 1] = bound->receiver;
            return call(bound->method, argCount);
        }
        case OBJ_CLASS: {
            ObjClass* klass = AS_CLASS(callee);
            vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
            Value initializer;
            if (tableGet(&klass->methods, vm.initString, &initializer)) {
                return call(AS_CLOSURE(initializer), argCount);
            } else if (argCount != 0) {
                runtimeError("Expected 0 arguments but got %d.", argCount);
                return false;
            }
            return true;
        }
        case OBJ_CLOSURE: {
            return call(AS_CLOSURE(callee), argCount);
        }
        case OBJ_NATIVE: {
            ObjNative* native = AS_NATIVE(callee);
            if (native->arity != -1 && native->arity != argCount) {
                runtimeError("Expected %d arguments but got %d.", native->arity, argCount);
                return false;
            }
            Value result;
            bool error = native->function(argCount, vm.stackTop - argCount, &result);
            if (error) {
                return !error;
            }
            vm.stackTop -= argCount + 1;
            push(result);
            return true;
        }
        case OBJ_NATIVE_CLASS: {
            ObjNativeClass* nklass = (ObjNativeClass*) AS_OBJ(callee);
            if (nklass->arity != -1 && nklass->arity != argCount) {
                runtimeError("Expected %d arguments but got %d.", nklass->arity, argCount);
                return false;
            }
            void* data = nklass->constructor(argCount, vm.stackTop - argCount);
            if (!data)
                return false; // constructor called runtimeError
            ObjNativeInstance* inst = newNativeInstance(nklass, data);
            vm.stackTop -= argCount + 1;
            push(OBJ_VAL(inst));
            return true;
        }
        default:
            break; // Non-callable object type.
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
}

static bool invokeFromClass(ObjClass* klass, ObjString* name, int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
    Value receiver = peek(argCount);

    if (IS_MODULE(receiver)) {
        ObjModule* module = AS_MODULE(receiver);
        Value value;
        if (!tableGet(&module->exports, name, &value)) {
            runtimeError("Undefined export '%s' in module '%s'.", name->chars, module->name->chars);
            return false;
        }
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }

    if (IS_NATIVE_INSTANCE(receiver)) {
        ObjNativeInstance* ni = AS_NATIVE_INSTANCE(receiver);
        for (const NativeMethod* m = ni->klass->methods; m && m->name; m++) {
            if (strcmp(m->name, name->chars) == 0) {
                Value result;
                bool error = m->fn(ni->data, argCount, vm.stackTop - argCount, &result);
                if (error)
                    return false;
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
        }
        runtimeError("Undefined method '%s' on %s.", name->chars, ni->klass->name);
        return false;
    }

    if (IS_LIST(receiver)) {
        ObjList* list = AS_LIST(receiver);
        for (const NativeMethod* m = listMethods; m && m->name; m++) {
            if (strcmp(m->name, name->chars) == 0) {
                Value result;
                bool error = m->fn((void*) list, argCount, vm.stackTop - argCount, &result);
                if (error)
                    return false;
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
        }
        runtimeError("Undefined method '%s' on list.", name->chars);
        return false;
    }

    if (IS_MAP(receiver)) {
        ObjMap* map = AS_MAP(receiver);
        for (const NativeMethod* m = mapMethods; m && m->name; m++) {
            if (strcmp(m->name, name->chars) == 0) {
                Value result;
                bool error = m->fn((void*) map, argCount, vm.stackTop - argCount, &result);
                if (error)
                    return false;
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
        }
        runtimeError("Undefined method '%s' on map.", name->chars);
        return false;
    }

    if (IS_STRING(receiver)) {
        ObjString* str = AS_STRING(receiver);
        for (const NativeMethod* m = stringMethods; m && m->name; m++) {
            if (strcmp(m->name, name->chars) == 0) {
                Value result;
                bool error = m->fn((void*) str, argCount, vm.stackTop - argCount, &result);
                if (error)
                    return false;
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
        }
        runtimeError("Undefined method '%s' on string.", name->chars);
        return false;
    }

    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }
    ObjInstance* instance = AS_INSTANCE(receiver);
    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }
    return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;
    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    ObjUpvalue* createdUpvalue = newUpvalue(local);

    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void defineMethod(ObjString* name) {
    Value method = peek(0);
    ObjClass* klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void) {
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, (size_t) a->length);
    memcpy(chars + a->length, b->chars, (size_t) b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static char* readModuleFile(const char* filePath) {
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        return NULL;
    }
    fseek(file, 0L, SEEK_END);
    long fileSizeLong = ftell(file);
    rewind(file);
    size_t fileSize = (size_t) fileSizeLong;
    char* buffer = (char*) malloc(fileSize + 1);
    if (buffer == NULL)
        return NULL;
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

static InterpretResult run(void) {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define CURRENT_GLOBALS()                                                                          \
    (frame->closure->function->module != NULL ? &frame->closure->function->module->exports         \
                                              : &vm.globals)
#define BINARY_OP(valueType, op)                                                                   \
    do {                                                                                           \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                                          \
            runtimeError("Operands must be numbers\nExpected number but got %s and %s.",           \
                         getType(peek(0)), getType(peek(1)));                                      \
            return INTERPRET_RUNTIME_ERROR;                                                        \
        }                                                                                          \
        double b = AS_NUMBER(pop());                                                               \
        double a = AS_NUMBER(pop());                                                               \
        push(valueType(a op b));                                                                   \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->closure->function->chunk,
                               (int) (frame->ip - frame->closure->function->chunk.code));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
        case OP_CONSTANT: {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_TRUE: {
            push(BOOL_VAL(true));
            break;
        }
        case OP_FALSE: {
            push(BOOL_VAL(false));
            break;
        }
        case OP_NIL: {
            push(NIL_VAL);
            break;
        }
        case OP_POP: {
            pop();
            break;
        }
        case OP_GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            break;
        }
        case OP_SET_LOCAL: {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_GET_GLOBAL: {
            ObjString* name = READ_STRING();
            Value value;
            if (!tableGet(CURRENT_GLOBALS(), name, &value) &&
                !tableGet(&vm.globals, name, &value)) { // Fallback to built-in natives like clock()
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }
        case OP_DEFINE_GLOBAL: {
            ObjString* name = READ_STRING();
            tableSet(CURRENT_GLOBALS(), name, peek(0));
            pop();
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString* name = READ_STRING();
            Table* targetTable = CURRENT_GLOBALS();
            if (tableSet(targetTable, name, peek(0))) {
                tableDelete(targetTable, name);
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_GET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case OP_SET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            break;
        }
        case OP_GET_PROPERTY: {
            if (IS_MODULE(peek(0))) {
                ObjModule* module = AS_MODULE(peek(0));
                ObjString* name = READ_STRING();
                Value value;
                if (tableGet(&module->exports, name, &value)) {
                    pop();
                    push(value);
                    break;
                }
                runtimeError("Undefined export '%s' in module.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            if (!IS_INSTANCE(peek(0))) {
                runtimeError("Only instances have properties.");
                return INTERPRET_RUNTIME_ERROR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(0));
            ObjString* name = READ_STRING();

            Value value;
            if (tableGet(&instance->fields, name, &value)) {
                pop(); // Instance.
                push(value);
                break;
            }
            if (!bindMethod(instance->klass, name)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SET_PROPERTY: {
            if (!IS_INSTANCE(peek(1))) {
                runtimeError("Only instances have fields.");
                return INTERPRET_RUNTIME_ERROR;
            }
            ObjInstance* instance = AS_INSTANCE(peek(1));
            tableSet(&instance->fields, READ_STRING(), peek(0));
            Value value = pop();
            pop();
            push(value);
            break;
        }
        case OP_GET_SUPER: {
            ObjString* name = READ_STRING();
            ObjClass* superclass = AS_CLASS(pop());

            if (!bindMethod(superclass, name)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_EQUAL: {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER: {
            BINARY_OP(BOOL_VAL, >);
            break;
        }
        case OP_LESS: {
            BINARY_OP(BOOL_VAL, <);
            break;
        }
        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                concatenate();
            } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            } else {
                runtimeError("Operands must be of same type");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT: {
            BINARY_OP(NUMBER_VAL, -);
            break;
        }
        case OP_MULTIPLY: {
            BINARY_OP(NUMBER_VAL, *);
            break;
        }
        case OP_MODULO: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            if (b == 0) {
                runtimeError("Modulo by zero.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL((double) ((long) a % (long) b)));
            break;
        }
        case OP_BITWISE_AND: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            long b = (long) AS_NUMBER(pop());
            long a = (long) AS_NUMBER(pop());
            push(NUMBER_VAL((double) (a & b)));
            break;
        }
        case OP_BITWISE_OR: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            long b = (long) AS_NUMBER(pop());
            long a = (long) AS_NUMBER(pop());
            push(NUMBER_VAL((double) (a | b)));
            break;
        }
        case OP_BITWISE_XOR: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            long b = (long) AS_NUMBER(pop());
            long a = (long) AS_NUMBER(pop());
            push(NUMBER_VAL((double) (a ^ b)));
            break;
        }
        case OP_BITWISE_SHL: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            long b = (long) AS_NUMBER(pop());
            long a = (long) AS_NUMBER(pop());
            push(NUMBER_VAL((double) (a << b)));
            break;
        }
        case OP_BITWISE_SHR: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            long b = (long) AS_NUMBER(pop());
            long a = (long) AS_NUMBER(pop());
            push(NUMBER_VAL((double) (a >> b)));
            break;
        }
        case OP_BITWISE_NOT: {
            if (!IS_NUMBER(peek(0))) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            long a = (long) AS_NUMBER(pop());
            push(NUMBER_VAL((double) (~a)));
            break;
        }
        case OP_DIVIDE: {
            BINARY_OP(NUMBER_VAL, /);
            break;
        }
        case OP_NOT: {
            push(BOOL_VAL(isFalsey(pop())));
            break;
        }
        case OP_NEGATE: {
            if (!IS_NUMBER(peek(0))) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        }
        case OP_PRINT: {
            printValue(pop());
            printf("\n");
            break;
        }
        case OP_JUMP: {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0)))
                frame->ip += offset;
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_STORE_SWITCH: {
            vm.switchValue = pop();
            break;
        }
        case OP_COMPARE_SWITCH: {
            Value caseValue = pop();
            push(BOOL_VAL(valuesEqual(vm.switchValue, caseValue)));
            break;
        }
        case OP_CALL: {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_INVOKE: {
            ObjString* method = READ_STRING();
            int argCount = READ_BYTE();
            if (!invoke(method, argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_SUPER_INVOKE: {
            ObjString* method = READ_STRING();
            int argCount = READ_BYTE();
            ObjClass* superclass = AS_CLASS(pop());
            if (!invokeFromClass(superclass, method, argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_CLOSURE: {
            ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
            ObjClosure* closure = newClosure(function);
            push(OBJ_VAL(closure));
            for (int i = 0; i < closure->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) {
                    closure->upvalues[i] = captureUpvalue(frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            break;
        }
        case OP_CLOSE_UPVALUE: {
            closeUpvalues(vm.stackTop - 1);
            pop();
            break;
        }
        case OP_RETURN: {
            Value result = pop();
            closeUpvalues(frame->slots);

            if (frame->closure->function->name == NULL &&
                frame->closure->function->module != NULL) {
                result = frame->slots[-1]; // Pulls the module we pushed in OP_IMPORT
                printf("%d\n", IS_MODULE(result));
            }

            vm.frameCount--;
            while (vm.handlerCount > 0 &&
                   vm.handlers[vm.handlerCount - 1].frameCount > vm.frameCount) {
                vm.handlerCount--;
            }
            if (vm.frameCount == 0) {
                pop();
                return INTERPRET_OK;
            }

            vm.stackTop = frame->slots - 1; // Pops both frame slots and the temporary module slot!
            push(result);
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_CLASS: {
            push(OBJ_VAL(newClass(READ_STRING())));
            break;
        }
        case OP_INHERIT: {
            Value superclass = peek(1);
            if (!IS_CLASS(superclass)) {
                runtimeError("Superclass must be a class.");
                return INTERPRET_RUNTIME_ERROR;
            }
            ObjClass* subclass = AS_CLASS(peek(0));
            tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
            pop();
            break;
        }
        case OP_METHOD: {
            defineMethod(READ_STRING());
            break;
        }
        case OP_BUILD_LIST: {
            int itemCount = READ_BYTE();
            ObjList* list = newList();
            // Push list onto the stack so DEBUG_STRESS_GC doesn't sweep it during writeValueArray!
            push(OBJ_VAL(list));
            for (int i = itemCount; i > 0; i--) {
                writeValueArray(&list->elements, peek(i));
            }
            pop();                    // Pop the temporary list protection slot
            vm.stackTop -= itemCount; // Pop all elements
            push(OBJ_VAL(list));
            break;
        }
        case OP_BUILD_MAP: {
            int entryCount = READ_BYTE();
            ObjMap* map = newMap();
            push(OBJ_VAL(map)); // Protect map from GC during tableSet!

            // Stack layout: [ ..., key_1, val_1, ..., key_n, val_n, map ]
            for (int i = entryCount; i > 0; i--) {
                Value key = peek(i * 2);
                Value val = peek(i * 2 - 1);
                if (!IS_STRING(key)) {
                    runtimeError("Map key must be a string, got %s.", getType(key));
                    return INTERPRET_RUNTIME_ERROR;
                }
                tableSet(&map->items, AS_STRING(key), val);
            }

            pop();                         // Pop temporary map slot
            vm.stackTop -= entryCount * 2; // Pop all keys and values
            push(OBJ_VAL(map));
            break;
        }
        case OP_INDEX_GET: {
            if (IS_LIST(peek(1))) {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("List index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = (int) AS_NUMBER(pop());
                ObjList* list = AS_LIST(pop());

                if (index < 0)
                    index += list->elements.count;
                if (index < 0 || index >= list->elements.count) {
                    runtimeError("List index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(list->elements.values[index]);
            } else if (IS_MAP(peek(1))) {
                if (!IS_STRING(peek(0))) {
                    runtimeError("Map key must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjString* key = AS_STRING(pop());
                ObjMap* map = AS_MAP(pop());
                Value value;
                if (!tableGet(&map->items, key, &value)) {
                    push(NIL_VAL); // Or runtimeError if you prefer strict key lookups
                } else {
                    push(value);
                }
            } else if (IS_STRING(peek(1))) {
                if (!IS_NUMBER(peek(0))) {
                    runtimeError("String index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                int index = (int) AS_NUMBER(pop());
                ObjString* str = AS_STRING(pop());

                if (index < 0)
                    index += str->length;
                if (index < 0 || index >= str->length) {
                    runtimeError("String index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(OBJ_VAL(copyString(&str->chars[index], 1)));
            } else {
                runtimeError("Can only index into lists and maps.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_INDEX_SET: {
            if (IS_LIST(peek(2))) {
                if (!IS_NUMBER(peek(1))) {
                    runtimeError("List index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value value = pop();
                int index = (int) AS_NUMBER(pop());
                ObjList* list = AS_LIST(pop());

                if (index < 0)
                    index += list->elements.count;
                if (index < 0 || index >= list->elements.count) {
                    runtimeError("List index out of bounds.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                list->elements.values[index] = value;
                push(value);
            } else if (IS_MAP(peek(2))) {
                if (!IS_STRING(peek(1))) {
                    runtimeError("Map key must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value value = pop();
                ObjString* key = AS_STRING(pop());
                ObjMap* map = AS_MAP(pop());
                tableSet(&map->items, key, value);
                push(value);
            } else {
                runtimeError("Can only subscript assign into lists and maps.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SLICE: {
            Value endVal = peek(0);
            Value startVal = peek(1);
            Value target = peek(2);

            if (!IS_NIL(startVal) && !IS_NUMBER(startVal)) {
                runtimeError("Slice start index must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            if (!IS_NIL(endVal) && !IS_NUMBER(endVal)) {
                runtimeError("Slice end index must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }

            if (IS_STRING(target)) {
                ObjString* str = AS_STRING(target);
                int len = str->length;
                int start = IS_NIL(startVal) ? 0 : (int) AS_NUMBER(startVal);
                int end = IS_NIL(endVal) ? len : (int) AS_NUMBER(endVal);

                if (start < 0)
                    start += len;
                if (end < 0)
                    end += len;
                if (start < 0)
                    start = 0;
                if (end > len)
                    end = len;
                if (start > end)
                    start = end;

                ObjString* sliced = copyString(str->chars + start, end - start);
                pop(); // end
                pop(); // start
                pop(); // target
                push(OBJ_VAL(sliced));
            } else if (IS_LIST(target)) {
                ObjList* list = AS_LIST(target);
                int len = list->elements.count;
                int start = IS_NIL(startVal) ? 0 : (int) AS_NUMBER(startVal);
                int end = IS_NIL(endVal) ? len : (int) AS_NUMBER(endVal);

                if (start < 0)
                    start += len;
                if (end < 0)
                    end += len;
                if (start < 0)
                    start = 0;
                if (end > len)
                    end = len;
                if (start > end)
                    start = end;

                ObjList* sliced = newList();
                push(OBJ_VAL(sliced)); // GC root protection
                for (int i = start; i < end; i++) {
                    writeValueArray(&sliced->elements, list->elements.values[i]);
                }
                pop(); // sliced
                pop(); // end
                pop(); // start
                pop(); // target
                push(OBJ_VAL(sliced));
            } else {
                runtimeError("Can only slice strings and lists.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_TO_STRING: {
            ObjString* str = valueToString(peek(0));
            pop();
            push(OBJ_VAL(str));
            break;
        }
        case OP_DUP: {
            push(peek(0));
            break;
        }
        case OP_DUP2: {
            Value b = peek(0);
            Value a = peek(1);
            push(a);
            push(b);
            break;
        }
        case OP_PUSH_HANDLER: {
            uint16_t offset = READ_SHORT();
            if (vm.handlerCount == HANDLERS_MAX) {
                runtimeError("Too many nested try blocks.");
                return INTERPRET_RUNTIME_ERROR;
            }
            ExceptionHandler* handler = &vm.handlers[vm.handlerCount++];
            handler->frameCount = vm.frameCount;
            handler->stackTop = vm.stackTop;
            handler->catchIp = frame->ip + offset;
            break;
        }
        case OP_POP_HANDLER: {
            vm.handlerCount--;
            break;
        }
        case OP_THROW: {
            Value thrown = pop();
            if (vm.handlerCount == 0) {
                // Unhandled exception! Print it as a runtime error with full stack trace
                ObjString* msg = valueToString(thrown);
                runtimeError("Unhandled exception: %s", msg->chars);
                return INTERPRET_RUNTIME_ERROR;
            }

            // Pop the nearest active handler
            ExceptionHandler handler = vm.handlers[--vm.handlerCount];

            // Close any open upvalues above the saved stackTop
            closeUpvalues(handler.stackTop);

            // Unwind the CallFrame stack and Value stack
            vm.frameCount = handler.frameCount;
            vm.stackTop = handler.stackTop;
            frame = &vm.frames[vm.frameCount - 1];
            frame->ip = handler.catchIp;

            // Push the thrown value onto the stack so the catch variable (err) receives it!
            push(thrown);
            break;
        }
        case OP_IMPORT: {
            ObjString* path = READ_STRING();
            Value moduleVal;
            if (tableGet(&vm.modules, path, &moduleVal)) {
                push(moduleVal);
                break;
            }
            char* source = readModuleFile(path->chars);
            if (source == NULL) {
                runtimeError("Could not open module file '%s'.", path->chars);
                return INTERPRET_RUNTIME_ERROR;
            }

            ObjModule* prevModule = vm.currentModule;
            ObjModule* module = newModule(path);
            vm.currentModule = module;

            tableSet(&vm.modules, path, OBJ_VAL(module));

            ObjFunction* function = compile(source);
            free(source);
            vm.currentModule = prevModule;

            if (function == NULL) {
                return INTERPRET_COMPILE_ERROR;
            }

            push(OBJ_VAL(module));
            push(OBJ_VAL(function));
            ObjClosure* closure = newClosure(function);
            pop();
            push(OBJ_VAL(closure));
            if (!call(closure, 0)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        }
    }

#undef CURRENT_GLOBALS
#undef BINARY_OP
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_SHORT
#undef READ_BYTE
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}