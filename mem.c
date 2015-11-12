/* allocator */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"

static alloc_cb _alloc_fn = malloc;
static free_cb _free_fn = free;

void set_bs_alloc(alloc_cb a) 
{
  _alloc_fn = a;
}

void set_bs_free(free_cb f) 
{
  _free_fn = f;
}

void *bs_alloc(size_t size) 
{
   return _alloc_fn(size);
}

void bs_free(void *ptr) 
{
  return _free_fn(ptr);
}

char *bs_strdup(const char * str) 
{
    char *new_str = (char*)bs_alloc(strlen(str) + 1);
    if (new_str != NULL) { 
        strcpy(new_str, str);
    }
    return new_str;
}
