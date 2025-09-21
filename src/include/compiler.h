#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"


ObjFunction* compile(const char* source);
void markCompilerRoots(void);

#endif