#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define PROFESSORS 5

pthread_mutex_t *chop_locks;
pthread_t *professor_threads;

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
		pthread_mutex_lock(&(professor_args->left_chopstick));
		printf("%s: Got left chopstick, thinking...\n", name);
		sleep(rand() % 3 + 1);
		printf("%s: Trying to use the right chopstick\n", name);
		pthread_mutex_lock(&(professor_args->right_chopstick));
		printf("%s: Aquired both chopsticks, eating\n", name);
		sleep(rand() % 11 + 10);
		pthread_mutex_unlock(&(professor_args->right_chopstick));
		pthread_mutex_unlock(&(professor_args->left_chopstick));
		printf("%s: Chopsticks released\n", name);
	}
}

int main(int argc, int argv) {
	char* names[] = {"Tanenbaum", "Bos", "Lamport", "Stallings", "Silberschatz"};
	professor_threads = malloc(PROFESSORS * sizeof(pthread_t));
	struct threadArgs *args = malloc(PROFESSORS * sizeof(struct threadArgs));
	chop_locks = malloc(PROFESSORS * sizeof(pthread_mutex_t));
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
	free(professor_threads);
	free(chop_locks);
	free(args);
}
