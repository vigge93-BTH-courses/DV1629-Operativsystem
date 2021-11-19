#include <stdio.h>
#include <stdlib.h>

struct Page {
	int adress;
	struct Page* next;
};

struct Page* head = NULL;
struct Page* tail = NULL;
int list_len = 0;

void insert_page(int adress) {
	struct Page *t = (struct Page*)malloc(sizeof(struct Page));
	t->adress = adress;
	t->next = NULL;

	if (head != NULL) {
		tail->next = t;
	} else {
		head = t;
	}
	tail = t;
	list_len++;
}

struct Page* find_page(int adress) {
	struct Page* t;
	t = head;

	while (t != NULL) {
		if(t->adress == adress) {
			return t;
		}
		t = t->next;
	}
	return NULL;
}

void remove_page(struct Page* page) {
	struct Page* t = head;
	struct Page* p;
	if (page->adress == head->adress) {
		t = head->next;
		free(head);
		head = t;
	} else {
		while (t != NULL && t->adress != page->adress) {
			p = t;
			t = t->next;
		}
		if (t->next == NULL) {
			p->next = NULL;
			free(t);
			tail = p;
		} else {
			p->next = t->next;
			free(t);
		}
	}
	list_len--;
}

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
		struct Page* t;

		t = find_page(page_adress);
		if (t != NULL) {
			remove_page(t);
			insert_page(page_adress);
		} else {
			pagefaults++;
			if (list_len == no_pages) { // All pages full, remove head
				remove_page(head);
			}
			insert_page(page_adress);
		}
	}
	free(line);
	fclose(file);

	printf("Pagefaults: %i\n", pagefaults);
	return 0;
}
