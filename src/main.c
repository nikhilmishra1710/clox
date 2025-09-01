#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc > 1) {
        printf("Arguments passed to the program:\n");
        for (int i = 1; i < argc; i++) {
            printf("Argument %d: %s\n", i, argv[i]);
        }
    } else {
        printf("No arguments passed to the program.\n");
    }
    
    int n,m;

    printf("Hello world starting with Clox project!\n");
    scanf("%d %d", &n, &m);
    printf("You entered: %d and %d\n", n, m);
    printf("Sum: %d\n", n + m);
    fprintf(stderr, "This is a sample error message\n");
    return 65;
}