#ifndef __BSMEM_H
#define __BSMEM_H

typedef void *(*alloc_cb)(size_t);
typedef void (*free_cb)(void *);

void set_bs_alloc(alloc_cb a); 
void set_bs_free(free_cb f); 
void *bs_alloc(size_t size);
void bs_free(void *ptr);
char *bs_strdup(const char * str);
 
#endif