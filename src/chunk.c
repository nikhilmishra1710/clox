#include "include/chunk.h"
#include "include/memory.h"

void initChunk(Chunk* chunk) {
    chunk->capacity     = 0;
    chunk->count        = 0;
    chunk->code         = NULL;
    chunk->lineCapacity = 0;
    chunk->lineCount    = 0;
    chunk->lines        = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    freeValueArray(&chunk->constants);
    FREE_ARRAY(LineInfo, chunk->lines, chunk->lineCapacity);
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code     = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if (chunk->lineCount > 0 && chunk->lines[chunk->lineCount - 1].line == line) {
        chunk->lines[chunk->lineCount - 1].count++;
    } else {
        if (chunk->lineCapacity < chunk->lineCount + 1) {
            int oldCapacity     = chunk->lineCapacity;
            chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
            chunk->lines = GROW_ARRAY(LineInfo, chunk->lines, oldCapacity, chunk->lineCapacity);
        }
        LineInfo lineData;
        lineData.line                  = line;
        lineData.count                 = 1;
        chunk->lines[chunk->lineCount] = lineData;
        chunk->lineCount++;
    }
}

int getLine(Chunk* chunk, int offset) {
    int running = 0;
    for (int i = 0; i < chunk->lineCount; i++) {
        running += chunk->lines[i].count;
        if (running > offset) {
            return chunk->lines[i].line;
        }
    }

    return 0;
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}