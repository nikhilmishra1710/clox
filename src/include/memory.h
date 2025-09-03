#ifndef CLOX_MEMORY_H
#define CLOX_MEMORY_H

#include "common.h"

#define GROW_CAPACITY(capacity) \
        ((capacity) < 8 ? 8 : capacity * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
        (type*)reallocate(pointer, sizeof(type) * (size_t)(oldCount), \
        sizeof(type) * (size_t)(newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
        reallocate(pointer, sizeof(type) * (size_t)(oldCount), 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif