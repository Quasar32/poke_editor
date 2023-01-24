#ifndef XSTD_H
#define XSTD_H

#include <stdlib.h>

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);

char *xstrdup(const char *s);

FILE *xfopen(const char *path, const char *mode);
void xfread_obj(FILE *f, void *buf, size_t size);
int xfgetc(FILE *f);
void xfseek(FILE *f, long off, int origin);
long xftell(FILE *f); 

long get_file_size(FILE *f);

#endif
