#include <stdio.h>
#include <stdlib.h>

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

	int pages[no_pages];
	for (int i = 0; i < no_pages; i++) {
		pages[i] = -1;
	}
	int first_page = 0;

	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		printf("Unable to open file: %s\n", filename);
		exit(-1);
	}

	int pagefaults = 0;
	size_t line_size = 0;
	char* line;
	ssize_t read_chars;

	while((read_chars = getline(&line, &line_size, file)) != -1) {
		int adress = atoi(line);
		int page_adress = adress - (adress % page_size); // Get the start of the page adresss
		int found = 0;
		for(int i = 0; i < no_pages; i++) {
			if (pages[i] == page_adress) {
				found = 1;
				break;
			}
		}
		if (found == 0) {
			pagefaults++;
			pages[first_page] = page_adress;
			first_page = (first_page + 1) % no_pages;
		}
	}
	free(line);
	fclose(file);

	printf("Pagefaults: %i\n", pagefaults);
	return 0;
}
