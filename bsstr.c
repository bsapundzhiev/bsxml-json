/* String utility
 *
 * Copyright (C) 2014 Borislav Sapundzhiev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "bsstr.h"

struct bsstr
{
    char *string;
    size_t length;
    size_t capacity;
};

#define STRING_BLOCK_SIZE 32

static bsstr * bsstr_realloc(bsstr *buf, size_t len);
static bsstr * bsstr_init(bsstr* str);

bsstr *bsstr_create(const char *data)
{
    bsstr *str = (bsstr *)malloc(sizeof (bsstr));
    if (!str) return NULL;

    bsstr_init(str);
    if (data) {
       bsstr_add(str, data);
    }
    return str;
}

bsstr *bsstr_init(bsstr* str)
{
    str->string = NULL;
    str->length = 0;
    str->capacity = STRING_BLOCK_SIZE;
    str->string = malloc(str->capacity);
    return str;
}

bsstr * bsstr_realloc(bsstr *buf, size_t len)
{
    static const size_t mask = ~(STRING_BLOCK_SIZE - 1);
    size_t newlen = buf->length + len + 1; /* add 1 for NUL */

    if (newlen > buf->capacity) {
        void *new_mem;
        buf->capacity = (newlen + (STRING_BLOCK_SIZE - 1)) & mask;
        //buf->string = (char *)realloc(buf->string, buf->capacity);
        new_mem  = malloc(buf->capacity);
        if (!new_mem) return NULL;
        memmove(new_mem, buf->string, buf->length);
        free(buf->string);
        buf->string = new_mem;
    }
    return buf;
}

void bsstr_add(bsstr* str, const char* string)
{
    size_t len = strlen(string);
    if (bsstr_realloc(str, len)) {
        strcpy(str->string + str->length, string);
        str->length += len;
        str->string[str->length] = '\0';        
    }
}

/*based on snprintf man */
void bsstr_printf(bsstr* buf, char* fmt, ...)
{
    int size = STRING_BLOCK_SIZE;  
    va_list ap;
    int end = buf->length;
    bsstr *p = bsstr_realloc(buf, size);
    if (p == NULL) {
        return;
    }
    while (1) {
        int n;
        va_start(ap, fmt);
        n = vsnprintf(buf->string + end, size, fmt, ap);
        va_end(ap);
        if (n > -1 && n < size) {
            buf->length = end + n;
            return;
        }
        if (n > -1)     
            size = n + 1; 
        else          
            size *= 2; 
        if((p = bsstr_realloc(buf, size)) == NULL ) {
            return;
        }
    }
}

void bsstr_addchr(bsstr* str, char ch)
{
    char s[] = {ch,'\0'}; 
    bsstr_add(str, s);
}

void bsstr_delete(bsstr *str)
{
    free(str->string);
    free(str);
}

char *bsstr_release(bsstr* str)
{
    char *result = str->string;
    free(str);
    return result;
}

char *bsstr_get_bufref(bsstr* str)
{
    return str->string;
}

int bsstr_length(bsstr *str)
{
    return str->length;
}

void bsstr_clear(bsstr* str)
{
    str->length = 0;
    str->string[0] = '\0';
}

char *bsstr_get_buf(bsstr* str)
{
    char *result = str->string;
    bsstr_init(str);
    return result;
}
