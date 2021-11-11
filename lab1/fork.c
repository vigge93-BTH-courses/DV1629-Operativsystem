#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    pid_t pidA;
    pid_t pidC;
    unsigned i;
    char letter;
    unsigned niterations = 1000;
    pidA = fork();
    if (pidA == 0) {
        letter = 'A';
     } else {
        pidC = fork();
        if (pidC == 0) {
            letter = 'C';
        } else {
            printf("\nPID child process 1 (A): %d\nPID child process 2 (C): %d\n", pidA, pidC);
            letter = 'B';
        }
    }
    for (i = 0; i < niterations; ++i)
        printf("%c = %d, ", letter, i);
    printf("\n");
}
