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
#ifndef _BSSTRING_H
#define _BSSTRING_H

#define isNullorEmpty(str)\
	(str == NULL || str[0] == '\0')

#define isAlpha(c)\
	((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))

#define isDigit(c)\
	(c >= '0' && c <= '9')

#define isAlphaNumeric(c)\
	(isDigit(c) || isAlpha(c))

typedef struct bsstr bsstr;

bsstr *bsstr_create(const char *data);
void bsstr_delete(bsstr* str) ;
void bsstr_printf(bsstr* str, char* format, ...);
void bsstr_add_size(bsstr* str, const char* string, int len);
void bsstr_add(bsstr* str, const char* string);
void bsstr_addchr(bsstr* str, char ch);
char *bsstr_get_buf(bsstr* str);
char *bsstr_get_copy(bsstr* str);
char* bsstr_get_bufref(bsstr* str);
/* return allocated buffer*/
char* bsstr_release(bsstr* str);
int bsstr_length(bsstr* str);
void bsstr_clear(bsstr* str);

#endif //_BSSTRING_H
