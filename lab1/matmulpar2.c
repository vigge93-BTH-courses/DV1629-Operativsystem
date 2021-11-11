/***************************************************************************
 *
 * Sequential version of Matrix-Matrix multiplication
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define SIZE 1024

static double a[SIZE][SIZE];
static double b[SIZE][SIZE];
static double c[SIZE][SIZE];

pthread_t handle[SIZE];

struct threadArgs {
	int i;
};

struct threadArgs mulArgs[SIZE];


void* init_thread(void* args) {
    struct threadArgs targs = *(struct threadArgs*)args;
    int row = targs.i;
    for(int j = 0; j < SIZE; j++) {
        a[row][j] = 1.0;
        b[row][j] = 1.0;
    }
}

static void
init_matrix(void)
{
    int i;

    for (i = 0; i < SIZE; i++){
        mulArgs[i].i = i;
        pthread_create(&handle[i], NULL, init_thread, (void*)&mulArgs[i]);
    }
    for(i = 0; i < SIZE; i++) {
        pthread_join(handle[i], NULL);
    }
}

static void
matmul_seq()
{
    int i, j, k;

    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            c[i][j] = 0.0;
            for (k = 0; k < SIZE; k++)
                c[i][j] = c[i][j] + a[i][k] * b[k][j];
        }
    }
}

void* matmul_thread(void* args) {
    struct threadArgs targs = *(struct threadArgs*)args;
    int row = targs.i;
    for (int j = 0; j < SIZE; j++) {
        c[row][j] = 0.0;
        for(int k = 0; k < SIZE; k++) {
            c[row][j] = c[row][j] + a[row][k] * b[k][j];
        }
    }
}

static void matmul_par() {
    for(int i = 0; i < SIZE; i++) {
        mulArgs[i].i = i;
        pthread_create(&handle[i], NULL, matmul_thread, (void*)&mulArgs[i]);
    }
    for(int i = 0; i < SIZE; i++) {
        pthread_join(handle[i], NULL);
    }
}

static void
print_matrix(void)
{
    int i, j;

    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++)
	        printf(" %7.2f", c[i][j]);
	    printf("\n");
    }
}

int
main(int argc, char **argv)
{
    init_matrix();
    matmul_par();
    // matmul_seq();
    // print_matrix();
}
