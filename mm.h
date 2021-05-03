#include <stdio.h>

extern int mm_init(void);
extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);

extern void print_blocks();
extern void print_free_list();


// Extra credit
extern void* mm_realloc(void* ptr, size_t size);

