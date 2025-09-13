#include "include/nativeFunction.h"
#include "time.h"

bool clockNative(int argCount, Value* args, Value* result) {
    (void) argCount;
    (void) args;
    *result = NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);

    return false;
}