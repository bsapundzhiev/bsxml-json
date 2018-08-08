/* array.c - data array
 *
 * Copyright (C) 2012 Borislav Sapundzhiev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"

static int
cpo_array_setsize(cpo_array_t *a, asize_t elements);

cpo_array_t *
cpo_array_create(asize_t size, asize_t elem_size)
{
    cpo_array_t *a = malloc(sizeof(cpo_array_t));
    if (a == NULL) {
        return NULL;
    }

    a->v = calloc(size, elem_size);
    a->num = 0;
    a->max = size;
    a->elem_size = elem_size;
    a->cmp = NULL;

    return a;
}

static int cpo_array_preallocate(cpo_array_t *a, asize_t elements)
{
    void *newv;
    asize_t newmax = a->max;

    while (elements >= newmax) {
        newmax = (newmax + 1);
    }

    newv = malloc(newmax * a->elem_size);

    if (newv == NULL) {
        return -1;
    }

    memcpy(newv, a->v, a->num * a->elem_size);

    if (a->v)
        free(a->v);

    a->v = newv;
    a->max = newmax;
    return 0;
}

static int cpo_array_setsize(cpo_array_t *a, asize_t elements)
{
    if (elements > a->max) {
        int result = cpo_array_preallocate(a, elements);
        if (result) {
            return result;
        }
    }

    a->num = elements;
    return 0;
}

void *
cpo_array_get_at(cpo_array_t *a, asize_t index)
{
    void *elt = NULL;

    if (index < a->num) {
        elt = (unsigned char*) a->v + a->elem_size * index;
    }
    return elt;
}

void *
cpo_array_push(cpo_array_t *a)
{
    void * elt = NULL;
    asize_t ix = a->num;

    int result = cpo_array_setsize(a, ix + 1);

    if (!result) {
        elt = (unsigned char*) a->v + a->elem_size * ix;
        a->cmp = NULL;
    }
    return elt;
}

void *
cpo_array_insert_at(cpo_array_t *a, asize_t index)
{
    void *elt = NULL;

    if (index <= a->num) {
        int result = cpo_array_setsize(a, a->num + 1);
        if (!result) {
            asize_t nmove = a->num - index - 1;
            memmove((unsigned char*)a->v + a->elem_size * (index + 1),
                    (unsigned char*)a->v + a->elem_size * index, nmove * a->elem_size);

            elt = (unsigned char*) a->v + a->elem_size * index;
            a->cmp = NULL;
        }
    }
    return elt;
}

void *
cpo_array_remove(cpo_array_t *a, asize_t index)
{
    void *elt = NULL;

    if (index < a->num) {
        asize_t nmove = a->num - index - 1;

        memmove((unsigned char*)a->v +a->elem_size * a->num,
                (unsigned char*)a->v +a->elem_size * index, a->elem_size);

        memmove((unsigned char*) a->v + a->elem_size * index,
                (unsigned char*) a->v + a->elem_size * (index + 1), nmove * a->elem_size);

        elt = (unsigned char*)a->v + a->elem_size * a->num;
        a->num--;
    }
    return elt;
}

void cpo_array_destroy(cpo_array_t *a)
{
    if (a->v) {
        free(a->v);
    }
    free(a);
}

void cpo_array_qsort(cpo_array_t *a, cmp_func cmp)
{
    qsort(a->v, a->num, a->elem_size, cmp);
}

void *cpo_array_bsearch(cpo_array_t *a, const void *key, cmp_func cmp)
{
    if (a->cmp == NULL || a->cmp != cmp) {
        cpo_array_qsort(a, cmp);
        a->cmp = cmp;
    }
    return bsearch(key, a->v, a->num, a->elem_size, cmp);
}

int array_cmp_int_asc(const void *a, const void *b)
{
    return (*(int*) a - *(int*) b);
}

int array_cmp_int_dsc(const void *a, const void *b)
{
    return (*(int*) b - *(int*) a);
}

int array_cmp_str_asc(const void *a, const void *b)
{
    return strcmp((char *) a, (char *) b);
}

int array_cmp_str_dsc(const void *a, const void *b)
{
    return strcmp((char *) b, (char *) a);
}

/* stack impl */
void * stack_push(cpo_array_t *stack)
{
    if (stack->num == stack->max) {
        fputs("Error: stack overflow\n", stderr);
        return NULL;
    }

    return cpo_array_insert_at(stack, 0);
}

void * stack_push_back(cpo_array_t *stack)
{
    if (stack->num == stack->max) {
        fputs("Error: stack overflow\n", stderr);
        return NULL;
    }

    return cpo_array_push(stack);
}

void * stack_back(cpo_array_t *stack)
{
    return cpo_array_get_at(stack, stack->num -1 );
}

void * stack_pop(cpo_array_t *stack)
{
    if (stack->num == 0) {
        fputs("Error: stack underflow\n", stderr);
        return NULL;
    }

    return  cpo_array_remove(stack, 0);
}

void * stack_pop_back(cpo_array_t *stack)
{
    if (stack->num == 0) {
        fputs("Error: stack underflow\n", stderr);
        return NULL;
    }

    return  cpo_array_remove(stack, stack->num -1);
}
