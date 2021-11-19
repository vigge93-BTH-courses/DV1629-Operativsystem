#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

struct Page {
	int adress;
	int time_until_next_access;
};

int main(int argc, char *argv[]) {
	if (argc < 4) {
		printf("Too few arguments\n");
		exit(-1);
	}
	char* no_pages_str = argv[1];
	char* page_size_str = argv[2];
	char* filename = argv[3];

	int page_size = atoi(page_size_str);
	int no_pages = atoi(no_pages_str);

	struct Page* pages[no_pages];
	for (int i = 0; i < no_pages; i++) {
		pages[i] = malloc(sizeof(struct Page));
		pages[i]->adress = -1;
		pages[i]->time_until_next_access = -1;
	}
	int adress_list_size = 16;
	struct Page** adresses = calloc(adress_list_size, sizeof(struct Page*));
	int adress_list_counter = 0;

	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		printf("Unable to open file: %s\n", filename);
		exit(-1);
	}

	size_t line_size = 0;
	char* line;
	ssize_t read_chars;

	while((read_chars = getline(&line, &line_size, file)) != -1) {
		int adress = atoi(line);
		int page_adress = adress - (adress % page_size); // Get the start of the page adresss
		adresses[adress_list_counter] = malloc(sizeof(struct Page));
		adresses[adress_list_counter]->adress = page_adress;
		adresses[adress_list_counter]->time_until_next_access = -1;

		for (int i = adress_list_counter - 1; i >= 0; i--) { // Find previous reference and update time
			if (adresses[i]->adress == page_adress && adresses[i]->time_until_next_access == -1) {
				adresses[i]->time_until_next_access = adress_list_counter - i;
				break;
			}
		}
		
		if (++adress_list_counter == adress_list_size) {
			adress_list_size *= 2;
			adresses = realloc(adresses, adress_list_size*sizeof(struct Page));
		}
	}

	int pagefaults = 0;
	for (int i = 0; i < adress_list_counter; i++) {
		int found = 0;
		for(int j = 0; j < no_pages; j++) {
			if (pages[j]->adress == adresses[i]->adress) {
				found = 1;
				free(pages[j]);
				pages[j] = adresses[i];
				break;
			}
		}
		if (found == 0) {
			pagefaults++;
			int best = 0;
			int best_score = INT_MIN;
			for (int j = 0; j < no_pages; j++) {
				if (pages[j]->time_until_next_access == -1 // A page that is never referenced again is always the best candidate
					|| (pages[j]->time_until_next_access > best_score && best_score != -1)) {
					best = j;
					best_score = pages[j]->time_until_next_access;
				}
			}
			free(pages[best]);
			pages[best] = adresses[i];
		}
		for(int j = 0; j < no_pages; j++) {
			if (pages[j]->time_until_next_access != -1)
				pages[j]->time_until_next_access--;
		}
	}

	for(int i = 0; i < no_pages; i++) {
		free(pages[i]);
	}
	free(line);
	free(adresses);
	fclose(file);

	printf("Pagefaults: %i\n", pagefaults);
	return 0;
}
