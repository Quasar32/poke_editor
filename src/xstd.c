#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xstd.h"

static void crt_print(const char *msg)
{
	fprintf(stderr, "crt error: %s: %s (%d)\n", 
			msg, strerror(errno), errno);
	exit(1);
}

static void crt_printf(const char *fmt, ...)
{
	va_list ap;
	char msg[1024];

	va_start(ap, fmt);
	vsprintf_s(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	crt_print(msg);
}

void *xmalloc(size_t size)
{
	void *ptr;

	if (size == 0) {
		return NULL;
	}

	ptr = malloc(size);
	if (!ptr) {
		crt_printf("xmalloc(%d)", size);
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
		crt_printf("xrealloc(%p, %d)", ptr, size);
	}

	return ptr;
}

char *xstrdup(const char *s)
{
	return strcpy(xmalloc(strlen(s) + 1), s);
}

FILE *xfopen(const char *path, const char *mode)
{
	FILE *f;
	f = fopen(path, mode);
	if (!f) {
		crt_printf("xfopen(%s, %s)", path, mode);
	}
	return f;
}

void xfseek(FILE *f, long off, int origin)
{
	if (fseek(f, off, origin) != 0) {
		crt_print("xfseek");
	}
}

long xftell(FILE *f) 
{
	long pos;
	pos = ftell(f);
	if (pos == -1L) {
		crt_print("xftell");
	}
	return pos;
}

void xfread_obj(FILE *f, void *buf, size_t size)
{
	if (fread(buf, size, 1, f) != 1 && size != 0) {
		crt_print("xfread_obj fail");
	}
}

int xfgetc(FILE *f)
{
	int ch;
	ch = fgetc(f);
	if (ch == EOF) {
		crt_print("xfgetc fail");
	}
	return ch;
}

long get_file_size(FILE *f)
{
	long pos;
	xfseek(f, 0, SEEK_END);
	pos = xftell(f);
	xfseek(f, 0, SEEK_SET);
	return pos;
}
