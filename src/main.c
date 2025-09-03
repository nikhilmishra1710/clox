#include "include/common.h"
#include "include/chunk.h"
#include "include/debug.h"

int main(void)
{
    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN, 1);

    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 2);
    writeChunk(&chunk, (uint8_t)constant, 2);

    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);

    return 0;
}
