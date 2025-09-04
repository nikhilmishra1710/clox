#include "include/common.h"
#include "include/chunk.h"
#include "include/debug.h"
#include "include/vm.h"

int main(void)
{
    initVM();

    Chunk chunk;
    initChunk(&chunk);
    
    int constant = addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 2);
    writeChunk(&chunk, (uint8_t)constant, 2);
    
    constant = addConstant(&chunk, 3.4);
    writeChunk(&chunk, OP_CONSTANT, 3);
    writeChunk(&chunk, (uint8_t)constant, 3);
    
    writeChunk(&chunk, OP_ADD, 3);
    
    constant = addConstant(&chunk, 5.6);
    writeChunk(&chunk, OP_CONSTANT, 3);
    writeChunk(&chunk, (uint8_t)constant, 3);
    
    writeChunk(&chunk, OP_DIVIDE, 4);
    writeChunk(&chunk, OP_NEGATE, 4);

    writeChunk(&chunk, OP_RETURN, 1);

    interpret(&chunk);
    freeVM();
    freeChunk(&chunk);

    return 0;
}
