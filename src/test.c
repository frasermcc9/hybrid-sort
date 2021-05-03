#include <stdio.h>
#include <unistd.h>

int main()
{
    int i = 2;
    fork();
    while (i > 0)
    {
        fork();
        printf("again\n");
        i--;
        fork();
    }
}