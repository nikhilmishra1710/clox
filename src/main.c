#include <stdio.h>

int main(void){
    int n,m;

    printf("Hello world starting with Clox project!\n");
    scanf("%d %d", &n, &m);
    printf("You entered: %d and %d\n", n, m);
    printf("Sum: %d\n", n + m);
    fprintf(stderr, "This is a sample error message\n");
    return 65;
}