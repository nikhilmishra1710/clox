#include "include/common.h"
#include "include/vm.h"

static void repl(void) {
#define MAX_LENGTH 1024
    char line[MAX_LENGTH];
    int  lineNo = 1;
    for (;;) {
        printf("In[%d]: ", lineNo);

        if (!fgets(line, MAX_LENGTH, stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
        lineNo += 1;
    }
#undef MAX_LENGTH
}

static char* readFile(const char* filePath) {
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) {
        fprintf(stderr, "Unable to open file please check %s\n", filePath);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    long fileSizeLong = ftell(file);
    if (fileSizeLong < 0) {
        fprintf(stderr, "ftell() failed.\n");
        exit(74);
    }
    size_t fileSize = (size_t) fileSizeLong;

    rewind(file);

    char* buffer = (char*) malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Unable to allocate memory free some up!!\n");
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Unable to read file %s. Please try again!\n", filePath);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char* filePath) {
    char*           source = readFile(filePath);
    InterpretResult result = interpret(source);

    if (result == INTERPRET_COMPILE_ERROR)
        exit(65);
    if (result == INTERPRET_RUNTIME_ERROR)
        exit(70);
}

int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "1. clox - REPL mode\n");
        fprintf(stderr, "2. clox [file] - run code from file\n");
        exit(64);
    }

    freeVM();
    return 0;
}
