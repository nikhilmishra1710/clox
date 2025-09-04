#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_RETURN
}OpCode;

typedef struct {
    int line;
    int count;
} LineInfo;
typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;

    int lineCount;
    int lineCapacity;
    LineInfo* lines;
}Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t code, int line);
int addConstant(Chunk* chunk, Value value);

int getLine(Chunk* chunk, int offset);

#endif