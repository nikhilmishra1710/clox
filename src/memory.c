#include "include/memory.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize)
{
    (void)oldSize; //Temporarily until oldSize is used
    if (newSize == 0)
    {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, newSize);
    if (result == NULL)
        exit(1);
    return result;
}