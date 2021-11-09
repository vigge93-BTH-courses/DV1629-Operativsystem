#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h> /* For O_* constants */

#define SHMSIZE 128
#define SHM_R 0400
#define SHM_W 0200
#define BUFFER_SIZE 10

const char *semName1 = "mutex_sema";
const char *semName2 = "empty_sema";
const char *semName3 = "full_sema";

int main(int argc, char **argv)
{
	struct shm_struct {
		int buffer[BUFFER_SIZE];
		unsigned in; /* Variables to keep track of buffer */
		unsigned out;
	};
	volatile struct shm_struct *shmp = NULL;
	char *addr = NULL;
	pid_t pid = -1;
	int var1 = 0, var2 = 0, shmid = -1, status;
	struct shmid_ds *shm_buf;
	sem_t *sem_mutex = sem_open(semName1, O_CREAT, O_RDWR, 1);
	sem_t *sem_empty = sem_open(semName2, O_CREAT, O_RDWR, BUFFER_SIZE);
	sem_t *sem_full = sem_open(semName3, O_CREAT, O_RDWR, 0);

	/* allocate a chunk of shared memory */
	shmid = shmget(IPC_PRIVATE, SHMSIZE, IPC_CREAT | SHM_R | SHM_W);
	shmp = (struct shm_struct *) shmat(shmid, addr, 0);
	shmp->in = 0;
	shmp->out = 0;
	pid = fork();

	if (pid != 0) {
		/* here's the parent, acting as producer */
		while (var1 < 100) {
			/* write to shmem */
			var1++;
			sem_wait(sem_empty);
			sem_wait(sem_mutex);
			printf("Sending %d\n", var1); fflush(stdout);
			shmp->buffer[shmp->in] = var1;
			shmp->in = (shmp->in + 1) % BUFFER_SIZE;
			sem_post(sem_mutex);
			sem_post(sem_full);
			sleep((rand() % 20) / 10);
		}
		shmdt(addr);
		shmctl(shmid, IPC_RMID, shm_buf);
		sem_close(sem_mutex);
		sem_close(sem_empty);
		sem_close(sem_full);
		wait(&status);
		sem_unlink(semName1);
		sem_unlink(semName2);
		sem_unlink(semName3);
	} else {
		/* here's the child, acting as consumer */
		while (var2 < 100) {
			/* read from shmem */
			sem_wait(sem_full);
			sem_wait(sem_mutex);
			var2 = shmp->buffer[shmp->out];
			printf("Received %d\n", var2); fflush(stdout);
			shmp->out = (shmp->out + 1) % BUFFER_SIZE;
			sem_post(sem_mutex);
			sem_post(sem_empty);
			sleep((rand() % 30) / 10);
		}
		shmdt(addr);
		shmctl(shmid, IPC_RMID, shm_buf);
		sem_close(sem_mutex);
		sem_close(sem_empty);
		sem_close(sem_full);
	}
}
