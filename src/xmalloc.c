#include <stdio.h>

#include "xmalloc.h"

void *xmalloc(size_t size)
{
	void *ptr;

	if (size == 0) {
		return NULL;
	}

	ptr = malloc(size);
	if (!ptr) {
		fprintf(stderr, "xmalloc fail\n");
		exit(1);
	}

	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	if (size == 0) {
		free(ptr);
		return NULL;
	}

	ptr = realloc(ptr, size); 
	if (!ptr) {
		fprintf(stderr, "xrealloc fail\n");
		exit(1);
	}

	return ptr;
}
