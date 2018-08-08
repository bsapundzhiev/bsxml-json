#ifndef _CPO_ARRAY_H
#define _CPO_ARRAY_H

#include <stdint.h>

#define ARR_VAL(p)  *((uintptr_t*)p)
#define ARR_VAL2PTR(v)  ((uintptr_t)(v))

typedef size_t asize_t;
typedef int (*cmp_func)(const void *, const void *) ;

typedef struct s_array {
    asize_t num;
    asize_t max;
    void *v;
    asize_t elem_size;
    cmp_func cmp;
} cpo_array_t;

cpo_array_t *
cpo_array_create(asize_t size, asize_t elem_size);

void *
cpo_array_get_at(cpo_array_t *a, asize_t index);

void *
cpo_array_push(cpo_array_t *a);

void *
cpo_array_insert_at(cpo_array_t *a, asize_t index);

void *
cpo_array_remove(cpo_array_t *a, asize_t index);

void
cpo_array_qsort(cpo_array_t *a, cmp_func cmp);

void *cpo_array_bsearch(cpo_array_t *a, const void *key, cmp_func cmp);

void
cpo_array_destroy(cpo_array_t *a);
/*stack impl */
void * stack_push(cpo_array_t *stack);
void * stack_pop(cpo_array_t *stack);
void * stack_pop_back(cpo_array_t *stack);
void * stack_push_back(cpo_array_t *stack);
void * stack_back(cpo_array_t *stack);

int array_cmp_int_asc(const void *a,  const void *b);
int array_cmp_int_dsc(const void *a,  const void *b);
int array_cmp_str_asc(const void *a,  const void *b);
int array_cmp_str_dsc(const void *a,  const void *b);

#endif
