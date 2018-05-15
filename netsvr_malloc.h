#ifndef netsvr_malloc_h
#define netsvr_malloc_h

#include <stddef.h>

#define netsvr_malloc malloc
#define netsvr_calloc calloc
#define netsvr_realloc realloc
#define netsvr_free free
#define netsvr_memalign memalign

void * netsvr_malloc(size_t sz);
void * netsvr_calloc(size_t nmemb,size_t size);
void * netsvr_realloc(void *ptr, size_t size);
void netsvr_free(void *ptr);
char * netsvr_strdup(const char *str);
void * netsvr_memalign(size_t alignment, size_t size);

#endif
