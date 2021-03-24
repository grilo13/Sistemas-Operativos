#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

void main(void)
{
    int f = fork();
    int x = fork();

    if((f*x) == 0){
        printf("B");

    } else {
        printf("A");
    }
}