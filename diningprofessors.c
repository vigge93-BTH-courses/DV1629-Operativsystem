#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

#define PROFESSORS 5

pthread_mutex_t *chop_locks;
pthread_t *professor_threads;
const char *semName = "singleChopsticks";
sem_t *sem_single_chops;

struct threadArgs {
	char* name;
	pthread_mutex_t left_chopstick;
	pthread_mutex_t right_chopstick;
};

void* professor_thread(void* args) {
	struct threadArgs *professor_args = (struct threadArgs*)args;
	char* name = professor_args->name;
	for (int i = 0; i < 10; i++) {
		printf("%s: Thinking...\n", name);
		sleep(rand() % 9 + 2);
		printf("%s: Trying to use left chopstick\n", name);
		sem_wait(sem_single_chops); // Decrease number of single chopsticks
		pthread_mutex_lock(&(professor_args->left_chopstick));
		printf("%s: Got left chopstick, thinking...\n", name);
		sleep(rand() % 3 + 1);
		printf("%s: Trying to use the right chopstick\n", name);
		pthread_mutex_lock(&(professor_args->right_chopstick));
		sem_post(sem_single_chops); // Increase number of single chopsticks that will be available
		printf("%s: Aquired both chopsticks, eating\n", name);
		sleep(rand() % 11 + 10);
		pthread_mutex_unlock(&(professor_args->right_chopstick));
		pthread_mutex_unlock(&(professor_args->left_chopstick));
		printf("%s: Chopsticks released\n", name);
	}
}

int main(int argc, int argv) {
	int status;
	char* names[] = {"Tanenbaum", "Bos", "Lamport", "Stallings", "Silberschatz"};
	professor_threads = malloc(PROFESSORS * sizeof(pthread_t));
	struct threadArgs *args = malloc(PROFESSORS * sizeof(struct threadArgs));
	chop_locks = malloc(PROFESSORS * sizeof(pthread_mutex_t));
	sem_single_chops = sem_open(semName, O_CREAT, O_RDWR, PROFESSORS - 1);
	for (int i = 0; i < PROFESSORS; i++) {
		pthread_mutex_init(&chop_locks[i], NULL);
	}
	for (int i = 0; i < PROFESSORS; i++) {
		args[i].name = names[i];
		args[i].left_chopstick = chop_locks[i];
		args[i].right_chopstick = chop_locks[(i + 1) % PROFESSORS];
		pthread_create(&professor_threads[i], NULL, professor_thread, &args[i]);
	}
	for (int i = 0; i < PROFESSORS; i++) {
		pthread_join(professor_threads[i], NULL);
	}
	for(int i = 0; i < PROFESSORS; i++) {
		pthread_mutex_destroy(&chop_locks[i]);
	}
	sem_close(sem_single_chops);
	wait(&status);
	sem_unlink(semName);
	free(professor_threads);
	free(chop_locks);
	free(args);
}
