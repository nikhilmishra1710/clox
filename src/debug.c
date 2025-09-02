#include "include/debug.h"

void disassembleChunk(Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);
    
    for(lu offset = 0; offset < chunk->count; )
    {
        offset = disassembleInstruction(chunk, offset);
    }
}

static lu simpleInstruction(const char* name, lu offset)
{
    printf("%s\n", name);
    return offset+1;
}

lu disassembleInstruction(Chunk* chunk, lu offset)
{
    printf("%04ld ", offset);

    uint8_t instruction = chunk->code[offset];
    switch(instruction)
    {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown OpCode %d\n", instruction);
            return offset+1;
    }
}
