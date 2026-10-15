#include "include/nativeFunction.h"
#include "include/object.h"
#include "include/vm.h"
#include "time.h"

void runtimeError(const char* format, ...);

static Value makeString(const char* chars, int length) {
    return OBJ_VAL(copyString(chars, length));
}

bool clockNative(int argCount, Value* args, Value* result) {
    (void) argCount;
    (void) args;
    *result = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);

    return false;
}

bool inputNative(int argCount, Value* args, Value* result) {
    (void) argCount;
    (void) args;
    if (argCount == 1) {
        if (IS_STRING(args[0])) {
            printf("%s", AS_CSTRING(args[0]));
        }
    }
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        *result = NIL_VAL;
        return false;
    }

    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    *result = OBJ_VAL(copyString(buffer, (int) strlen(buffer)));
    return false;
}

static void* fileConstructor(int argCount, Value* args) {
    if (argCount < 1 || argCount > 2) {
        runtimeError("File() expects 1 or 2 arguments (path [, mode]).");
        return NULL;
    }
    if (!IS_STRING(args[0])) {
        runtimeError("File(): path must be a string.");
        return NULL;
    }
    const char* path = AS_CSTRING(args[0]);
    const char* mode = "r";
    if (argCount == 2) {
        if (!IS_STRING(args[1])) {
            runtimeError("File(): mode must be a string.");
            return NULL;
        }
        mode = AS_CSTRING(args[1]);
    }
    FILE* f = fopen(path, mode);
    if (!f) {
        runtimeError("File(): cannot open '%s' with mode '%s'.", path, mode);
        return NULL;
    }
    return (void*) f;
}

static void fileFinalizer(void* data) {
    if (data)
        fclose((FILE*) data);
}

static bool fileRead(void* data, int ac, Value* args, Value* result) {
    (void) ac;
    (void) args;
    FILE* f = (FILE*) data;
    if (!f) {
        runtimeError("File.read(): file is closed.");
        return true;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf) {
        runtimeError("File.read(): out of memory.");
        return true;
    }
    size_t nread = fread(buf, 1, (size_t) sz, f);
    buf[nread] = '\0';
    *result = makeString(buf, (int) sz);
    free(buf);
    return false;
}

static bool fileReadline(void* data, int ac, Value* args, Value* result) {
    (void) ac;
    (void) args;
    FILE* f = (FILE*) data;
    if (!f) {
        runtimeError("File.readline(): file is closed.");
        return true;
    }
    char buf[4096];
    if (!fgets(buf, sizeof(buf), f)) {
        *result = NIL_VAL; // EOF
        return false;
    }
    *result = makeString(buf, (int) strlen(buf));
    return false;
}

static bool fileWrite(void* data, int ac, Value* args, Value* result) {
    (void) ac;
    FILE* f = (FILE*) data;
    if (!f) {
        runtimeError("File.write(): file is closed.");
        return true;
    }
    if (!IS_STRING(args[0])) {
        runtimeError("File.write() expects a string.");
        return true;
    }
    fputs(AS_CSTRING(args[0]), f);
    *result = NIL_VAL;
    return false;
}

static bool fileClose(void* data, int ac, Value* args, Value* result) {
    (void) ac;
    (void) args;
    FILE* f = (FILE*) data;
    if (f)
        fclose(f);
    // We can't mutate `data` directly here since it's passed by value,
    // but we CAN reach back through args[-1] which is the receiver on the stack.
    // Simpler: null it via the instance pointer sitting just below the args.
    // args points to arg[0]; the receiver is at args[-1] on the VM stack.
    if (IS_NATIVE_INSTANCE(args[-1])) {
        AS_NATIVE_INSTANCE(args[-1])->data = NULL;
    }
    *result = NIL_VAL;
    return false;
}

static bool fileIsEof(void* data, int ac, Value* args, Value* result) {
    (void) ac;
    (void) args;
    FILE* f = (FILE*) data;
    *result = BOOL_VAL(f == NULL || feof(f));
    return false;
}

// NULL-terminated method table
static const NativeMethod fileMethods[] = {
    {"read", fileRead},   {"readline", fileReadline}, {"write", fileWrite},
    {"close", fileClose}, {"is_eof", fileIsEof},      {NULL, NULL} // sentinel
};

// Static class definition — lives for the lifetime of the program
ObjNativeClass fileClass = {
    .obj = {0}, // initialised by defineNativeClass() in vm.c
    .name = "File",
    .constructor = fileConstructor,
    .finalizer = fileFinalizer,
    .methods = fileMethods,
    .arity = -1, // variadic: 1 or 2 args
};

static bool listPush(void* data, int argCount, Value* args, Value* result) {
    ObjList* list = (ObjList*) data;
    if (argCount != 1) {
        runtimeError("List.push() expects 1 argument but got %d.", argCount);
        return true;
    }
    writeValueArray(&list->elements, args[0]);
    *result = NIL_VAL;
    return false;
}

static bool listPop(void* data, int argCount, Value* args, Value* result) {
    (void) args;
    ObjList* list = (ObjList*) data;
    if (argCount != 0) {
        runtimeError("List.pop() expects 0 arguments but got %d.", argCount);
        return true;
    }
    if (list->elements.count == 0) {
        runtimeError("List.pop(): cannot pop from an empty list.");
        return true;
    }
    *result = list->elements.values[--list->elements.count];
    return false;
}

static bool listLen(void* data, int argCount, Value* args, Value* result) {
    (void) args;
    ObjList* list = (ObjList*) data;
    if (argCount != 0) {
        runtimeError("List.len() expects 0 arguments but got %d.", argCount);
        return true;
    }
    *result = NUMBER_VAL((double) list->elements.count);
    return false;
}

const NativeMethod listMethods[] = {
    {"push", listPush}, {"pop", listPop}, {"len", listLen}, {NULL, NULL}};

static bool mapHas(void* data, int argCount, Value* args, Value* result) {
    ObjMap* map = (ObjMap*) data;
    if (argCount != 1 || !IS_STRING(args[0])) {
        runtimeError("Map.has() expects 1 string key argument.");
        return true;
    }
    Value dummy;
    *result = BOOL_VAL(tableGet(&map->items, AS_STRING(args[0]), &dummy));
    return false;
}

static bool mapRemove(void* data, int argCount, Value* args, Value* result) {
    ObjMap* map = (ObjMap*) data;
    if (argCount != 1 || !IS_STRING(args[0])) {
        runtimeError("Map.remove() expects 1 string key argument.");
        return true;
    }
    *result = BOOL_VAL(tableDelete(&map->items, AS_STRING(args[0])));
    return false;
}

static bool mapLen(void* data, int argCount, Value* args, Value* result) {
    (void) args;
    ObjMap* map = (ObjMap*) data;
    if (argCount != 0) {
        runtimeError("Map.len() expects 0 arguments.");
        return true;
    }
    int activeCount = 0;
    for (int i = 0; i < map->items.capacity; i++) {
        if (map->items.entries[i].key != NULL) {
            activeCount++;
        }
    }
    *result = NUMBER_VAL((double) activeCount);
    return false;
}

static bool mapKeys(void* data, int argCount, Value* args, Value* result) {
    (void) args;
    ObjMap* map = (ObjMap*) data;
    if (argCount != 0) {
        runtimeError("Map.keys() expects 0 arguments.");
        return true;
    }
    ObjList* keys = newList();
    push(OBJ_VAL(keys)); // GC root protection while growing list
    for (int i = 0; i < map->items.capacity; i++) {
        if (map->items.entries[i].key != NULL) {
            writeValueArray(&keys->elements, OBJ_VAL(map->items.entries[i].key));
        }
    }
    pop();
    *result = OBJ_VAL(keys);
    return false;
}

const NativeMethod mapMethods[] = {
    {"has", mapHas}, {"remove", mapRemove}, {"len", mapLen}, {"keys", mapKeys}, {NULL, NULL}};

static bool stringLength(void* data, int argCount, Value* args, Value* result) {
    (void) args;
    ObjString* str = (ObjString*) data;
    if (argCount != 0) {
        runtimeError("String.length() expects 0 arguments.");
        return true;
    }
    *result = NUMBER_VAL((double) str->length);
    return false;
}

static bool stringSlice(void* data, int argCount, Value* args, Value* result) {
    ObjString* str = (ObjString*) data;
    if (argCount != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        runtimeError("String.slice(start, end) expects 2 number arguments.");
        return true;
    }
    int start = (int) AS_NUMBER(args[0]);
    int end = (int) AS_NUMBER(args[1]);
    if (start < 0)
        start += str->length;
    if (end < 0)
        end += str->length;
    if (start < 0)
        start = 0;
    if (end > str->length)
        end = str->length;
    if (start > end)
        start = end;

    *result = OBJ_VAL(copyString(str->chars + start, end - start));
    return false;
}

static bool stringFind(void* data, int argCount, Value* args, Value* result) {
    ObjString* str = (ObjString*) data;
    if (argCount != 1 || !IS_STRING(args[0])) {
        runtimeError("String.find() expects 1 string argument.");
        return true;
    }
    const char* match = strstr(str->chars, AS_CSTRING(args[0]));
    *result = NUMBER_VAL(match ? (double) (match - str->chars) : -1.0);
    return false;
}

const NativeMethod stringMethods[] = {{"length", stringLength},
                                      {"len", stringLength},
                                      {"slice", stringSlice},
                                      {"find", stringFind},
                                      {NULL, NULL}};