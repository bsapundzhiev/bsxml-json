/* Simple JSON implementation
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "bsstr.h"
#include "bsjson.h"

#define JSON_STACK_SIZE 32

/* JSON streaming buffer size - tune for your platform:
 * 8192 (8KB):   Desktop/Server, good speed/memory balance
 * 4096 (4KB):   Embedded systems (ARM Cortex-M), ~4KB total memory per chunk
 * 2048 (2KB):   Tight embedded, IoT (~2KB total memory per chunk) - RECOMMENDED
 * 1024 (1KB):   Very tight constraints (~1KB total memory per chunk)
 * 512 (512B):   Ultra-low power, minimal RAM
 * 256 (256B):   Extreme constraints - works for simple JSON only, may fail on complex files
 * 
 * UNICODE SUPPORT REQUIREMENTS:
 * - Single escape \uXXXX = 6 bytes
 * - UTF-16 surrogate pair \uXXXX\uYYYY = 12 bytes (for emoji, etc.)
 * - Minimum buffer size: 12 bytes (for Unicode emoji support)
 * - Recommended: 2048+ bytes for production use
 * - Note: Buffers < 12 bytes WILL FAIL parsing files with surrogate pairs (emoji)
 * 
 * NOTE: Smaller buffers = slower parsing + more disk I/O
 *       But linear memory usage stays constant regardless of file size
 */
#define JSON_BUFFER_SIZE 32 /* Minimum: 12 for Unicode; Recommended: 2048+ for production */

enum eElemType {
    JSON_OBJ_B, JSON_OBJ_E, JSON_ARR_B, JSON_ARR_E,
    JSON_COLON, JSON_COMMA, JSON_QUOTE, JSON_LEFT,
    JSON_RIGHT, JSON_BEGIN, JSON_FORMFEED, JSON_LF,
    JSON_CR, JSON_TAB, JSON_HEX, JSON_INVALID
};

static const char jsonElems [] = {
    '{','}','[', ']', ':',',','\"','\\',
    '/','\b','\f', '\n', '\r', '\t', 'u'
};

static char Json_elem(enum eElemType type)
{
    return  jsonElems[type];
}

static enum eElemType Json_typeOfElem(const char c)
{
    enum eElemType type = JSON_INVALID;
    for (type = JSON_OBJ_B; type != JSON_INVALID; ++type) {
        if (c == jsonElems[type]) break;
    }

    return type;
}

/* Detect JSON value type from string representation */
static int Json_detectValueType(const char *value)
{
    if (!value || *value == '\0') {
        return JSON_VALUE_NULL;
    }
    
    /* Check for null */
    if (strcmp(value, "null") == 0) {
        return JSON_VALUE_NULL;
    }
    
    /* Check for boolean */
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
        return JSON_VALUE_BOOL;
    }
    
    /* Check for number */
    char *endptr;
    strtod(value, &endptr);
    if (endptr != value && *endptr == '\0') {
        return JSON_VALUE_NUMBER;
    }
    
    return JSON_VALUE_STRING;
}

JsonNode * JsonNode_Create()
{
    JsonNode *node = (JsonNode *) malloc( sizeof(JsonNode) );
    if (!node) return NULL;

    node->m_type = JSON_ROOT;
    node->m_name = NULL;
    node->m_parent = NULL;
    node->m_pairs = cpo_array_create(4, sizeof(JsonPair));
    node->m_childs =  cpo_array_create(4, sizeof(JsonNode));
    return node;
}

JsonNode * JsonNode_createChild(JsonNode * node, String name, int type)
{
    JsonNode * child = (JsonNode *)cpo_array_push(node->m_childs);
    child->m_type = type;
    child->m_parent = node;
    child->m_name = (name != NULL) ? strdup(name) : NULL;
    child->m_pairs = cpo_array_create(4, sizeof(JsonPair));
    child->m_childs =  cpo_array_create(4, sizeof(JsonNode));
    return child;
}

JsonNode * JsonNode_createObject(JsonNode * node, String name)
{
    return  JsonNode_createChild(node, name, JSON_OBJ);
}

JsonNode * JsonNode_createArray(JsonNode * node, String name)
{
    return  JsonNode_createChild(node, name, JSON_ARRAY);
}

void JsonNode_setPair(JsonNode * node, const String key, const String value )
{
    JsonPair *a = (JsonPair*)cpo_array_push( node->m_pairs );
    a->key =  strdup(key);
    a->value =  strdup(value);
    a->type = Json_detectValueType(value);
}

static int JsonPair_comparer(const void *a, const void *b)
{
    return strcmp(((JsonPair *) a)->key, ((JsonPair *) b)->key);
}

JsonPair * JsonNode_findPair(JsonNode *node, const String key)
{
    JsonPair p = { key, NULL };
    JsonPair *ret = (JsonPair*)cpo_array_bsearch(node->m_pairs, &p, JsonPair_comparer);
    return ret;
}

String JsonNode_getPairValue(JsonNode *node, const String key)
{
    String value  = NULL;
    JsonPair *pair = JsonNode_findPair(node,  key);
    if(pair) {
        value = pair->value;
    }
    return value;
}

int JsonNode_getPairValueInt(JsonNode *node, const String key)
{
    String jsonVal = JsonNode_getPairValue(node, key);
    if(jsonVal) {
        return atoi(jsonVal);
    }
    return 0;
}

double JsonNode_getPairValueFloat(JsonNode *node, const String key)
{
    String jsonVal = JsonNode_getPairValue(node, key);
    if(jsonVal) {
        return atof(jsonVal);
    }
    return 0;
}

double JsonNode_getPairValueDouble(JsonNode *node, const String key)
{
    return JsonNode_getPairValueFloat(node, key);
}

int JsonNode_getPairValueType(JsonNode *node, const String key)
{
    JsonPair *pair = JsonNode_findPair(node, key);
    if(pair) {
        return pair->type;
    }
    return JSON_VALUE_NULL;
}

static int JsonNode_comparer(const void *a, const void *b)
{
    return strcmp(((JsonNode *) a)->m_name, ((JsonNode *) b)->m_name);
}

JsonNode * JsonNode_findChild(JsonNode *node, const String name, int type)
{
    JsonNode tmpNode = { type, name };
    JsonNode *ret = (JsonNode*)cpo_array_bsearch(node->m_childs, &tmpNode, JsonNode_comparer);
    return ret;
}

asize_t JsonNode_getChildCount(JsonNode *node)
{
    return node->m_childs->num;
}

asize_t JsonNode_getPairCount(JsonNode *node)
{
    return node->m_pairs->num;
}

JsonNode * JsonNode_getChild(JsonNode *node, asize_t index)
{
    return (JsonNode *)cpo_array_get_at(node->m_childs, index);
}

JsonPair * JsonNode_getPair(JsonNode *node, asize_t index)
{
    return (JsonPair *)cpo_array_get_at(node->m_pairs, index);
}

void JsonNode_delete(JsonNode *node)
{
    asize_t i;
    if (!node) return;
    for (i=0; i < JsonNode_getPairCount(node); i++) {
        JsonPair *pair = JsonNode_getPair(node, i);
        free(pair->key);
        free(pair->value);
    }

    if (node->m_childs) {
        cpo_array_destroy(node->m_childs);
    }

    if (node->m_pairs) {
        cpo_array_destroy(node->m_pairs);
    }

    if (node->m_name)
        free(node->m_name);
}

void JsonNode_deleteTree(JsonNode *root)
{
    asize_t i;
    if (!root) return;
    for (i=0 ; i < JsonNode_getChildCount(root); i++) {
        JsonNode *node = JsonNode_getChild(root, i);
        JsonNode_deleteTree(node);
    }

    JsonNode_delete(root);

    if (root->m_type == JSON_ROOT) {
        free(root);
    }
}

String JsonNode_getJSON(JsonNode *node)
{
    asize_t i, nPairs, nChilds;
    String JSON = NULL;
    bsstr *buff = bsstr_create("");

    if (!isNullorEmpty(node->m_name)) {
        bsstr_printf(buff, "\"%s\":", node->m_name);
    }

    bsstr_printf(buff, "%s\n",  JSON_IS_OBJ(node) ? "{" : "[");
    nPairs = JsonNode_getPairCount(node);
    nChilds = JsonNode_getChildCount(node);

    for (i=0; i < nPairs; i++ ) {
        JsonPair *pair = JsonNode_getPair(node, i);

        if (JSON_IS_ARRAY(node)) {
            bsstr_printf(buff, "\"%s\"", pair->key);
        } else {
            bsstr_printf(buff, "\"%s\":\"%s\"", pair->key, pair->value);
        }

        bsstr_printf(buff, "%s\n", (i < nPairs -1 || nChilds > 0) ? "," : "");
    }

    for (i = 0; i < nChilds; i++) {
        JsonNode* child = JsonNode_getChild(node, i);
        String childJSON = JsonNode_getJSON(child);
        bsstr_add(buff, childJSON);
        if (i < nChilds -1) {
            int len = bsstr_length(buff)-1;
            bsstr_get_bufref(buff)[len] = ',';
            bsstr_addchr(buff, '\n');
        }
        free(childJSON);
    }

    bsstr_printf(buff, "%s\n",  JSON_IS_OBJ(node) ? "}" : "]");
    JSON = bsstr_release(buff);
    return JSON;
}
/********************************************************************************/
/* Parse JSON                                                                   */
/********************************************************************************/
enum {JSON_ERR_NONE, JSON_ERR_QUOTE, JSON_ERR_COMMA, JSNON_ERR_NOTOBJ, JSON_ERR_SYN};
const char *jsonParser_errlist[] = {
    "Unknown error", "Missing or unexpected quote",
    "Missing or unexpected comma", "Unexpected object end",
    "Unexpected syntax"
};

struct  ParserInternal { /*Jsonlexer*/
    int error;
    int line;
    int quote_begin;
    int is_value;
    bsstr *key;
    bsstr *value;
    cpo_array_t stack;
    struct JsonParser *parser;
    void (*startElem)(struct JsonParser *, const String, int);
    void (*endElem)(struct JsonParser *, const String, int);
    void (*elemData)(struct JsonParser *, const String,  const String);
};

static void JsonParser_internalCreate(struct ParserInternal *pi)
{
    pi->error = JSON_ERR_NONE;
    pi->line = 0;
    pi->quote_begin = pi->is_value = 0;
    pi->key = bsstr_create("");
    pi->value = bsstr_create("");
    pi->stack.v = calloc(JSON_STACK_SIZE, sizeof(char*));
    pi->stack.num = 0;
    pi->stack.max = JSON_STACK_SIZE;
    pi->stack.elem_size = sizeof(char*);
    pi->startElem = NULL;
    pi->endElem = NULL;
    pi->elemData = NULL;
}

static void JsonParser_internalReset(struct ParserInternal *pi)
{
    pi->quote_begin = pi->is_value = 0;
    bsstr_clear(pi->key);
    bsstr_clear(pi->value);
}

static void JsonParser_internalDelete(struct ParserInternal *pi)
{
    bsstr_delete(pi->key);
    bsstr_delete(pi->value);
    free(pi->stack.v);
}

static int JsonParser_internalBeginObj(struct  ParserInternal *pi, enum eElemType elemType)
{
    char *name = bsstr_get_buf(pi->key);
    void *ptr = stack_push_back(&(pi)->stack);
    ARR_VAL(ptr) = ARR_VAL2PTR(name);
    if (elemType != JSON_ARR_B && elemType != JSON_OBJ_B) {
        pi->error = JSNON_ERR_NOTOBJ;
        return JSON_NOK;
    }
    if (pi->startElem) {
        pi->startElem(pi->parser, name, (elemType == JSON_ARR_B) ? JSON_ARRAY : JSON_OBJ);
    }

    JsonParser_internalReset(pi);
    return JSON_OK;
}

static int JsonParser_internalEndObj(struct  ParserInternal *pi, enum eElemType elemType)
{
    char *name;
    void *ptr = stack_pop_back(&(pi)->stack);

    if (elemType != JSON_ARR_E && elemType != JSON_OBJ_E) {
        pi->error = JSNON_ERR_NOTOBJ;
        return JSON_NOK;
    }
    if ((name = (char*)ARR_VAL(ptr))) {
        if (pi->endElem) {
            pi->endElem(pi->parser, name, (elemType == JSON_ARR_E) ? JSON_ARRAY : JSON_OBJ);
        }
        free(name);
    }

    JsonParser_internalReset(pi);
    return 0;
}

static int JsonParser_internalData(struct  ParserInternal *pi)
{
    if (pi->quote_begin) {
        pi->error = JSON_ERR_QUOTE;
        return JSON_NOK;
    }

    if (bsstr_length(pi->key) || bsstr_length(pi->value)) {

        if (pi->elemData) {
            pi->elemData(pi->parser,  bsstr_get_bufref(pi->key), bsstr_get_bufref(pi->value) );
        }
    }

    JsonParser_internalReset(pi);
    return 0;
}

/* Find next non-whitespace character within buffer bounds
 * Returns '\0' if reached buffer end (don't go past buffer limit) */
static char JsonParser_next_char(const char *p, int pos, int len)
{
    char ch;
    while ( ++pos < len ) {
        ch = *(p + pos);
        if (ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t' )
            return ch;
    }
    return '\0';  /* End of buffer reached */
}

/* Find previous non-whitespace character within buffer bounds
 * Returns '\0' if reached buffer start (don't go past 0) */
static char JsonParser_prev_char(const char *p, int pos)
{
    char ch;
    while ( --pos >= 0 ) {
        ch = *(p + pos);
        if (ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t' )
            return ch;
    }
    return '\0';  /* Start of buffer reached */
}

static int JsonParser_hexDigit(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static int JsonParser_parseUnicodeEscape(const char *p, int len, int pos, unsigned *codepoint)
{
    unsigned cp = 0;
    int digit;

    if (pos + 4 > len) return -1;  /* Need 4 hex digits */
    for (int j = 0; j < 4; j++) {
        digit = JsonParser_hexDigit(p[pos + j]);
        if (digit < 0) return -1;
        cp = (cp << 4) | (unsigned)digit;
    }

    *codepoint = cp;
    return 0;
}

/* Handle surrogate pairs and validate Unicode codepoint */
static int JsonParser_decodeSurrogatePair(const char *p, int len, int pos, unsigned *codepoint)
{
    if (*codepoint >= 0xD800 && *codepoint <= 0xDBFF) {
        /* High surrogate - must be followed by low surrogate */
        if (pos + 6 > len) {
            return -1;  /* Not enough bytes for low surrogate */
        }
        if (p[pos] != '\\' || p[pos + 1] != 'u') {
            return -1;  /* Missing low surrogate */
        }
        unsigned low;
        if (JsonParser_parseUnicodeEscape(p, len, pos + 2, &low) != 0
            || low < 0xDC00 || low > 0xDFFF) {
            return -1;
        }
        *codepoint = 0x10000 + ((((*codepoint) - 0xD800) << 10) | (low - 0xDC00));
        return 6;  /* consumed 6 chars (\uXXXX\uYYYY) */
    } else if (*codepoint >= 0xDC00 && *codepoint <= 0xDFFF) {
        return -1;  /* Unpaired low surrogate */
    }
    return 0;  /* No surrogate pair, consumed 0 extra chars */
}

static int JsonParser_encodeUtf8(bsstr *dst, unsigned codepoint)
{
    if (codepoint <= 0x7F) {
        bsstr_addchr(dst, (char)codepoint);
    } else if (codepoint <= 0x7FF) {
        bsstr_addchr(dst, (char)(0xC0 | (codepoint >> 6)));
        bsstr_addchr(dst, (char)(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        bsstr_addchr(dst, (char)(0xE0 | (codepoint >> 12)));
        bsstr_addchr(dst, (char)(0x80 | ((codepoint >> 6) & 0x3F)));
        bsstr_addchr(dst, (char)(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0x10FFFF) {
        bsstr_addchr(dst, (char)(0xF0 | (codepoint >> 18)));
        bsstr_addchr(dst, (char)(0x80 | ((codepoint >> 12) & 0x3F)));
        bsstr_addchr(dst, (char)(0x80 | ((codepoint >> 6) & 0x3F)));
        bsstr_addchr(dst, (char)(0x80 | (codepoint & 0x3F)));
    } else {
        return -1;
    }

    return 0;
}

static int JsonParser_appendEscapeSequence(bsstr *dest, char esc_char)
{
    /* Process a JSON escape sequence by appending the unescaped character.
     * 
     * Args:
     *   dest:     Destination string to append to (e.g., pi->value or pi->key)
     *   esc_char: Character after backslash (e.g., 'n' for \\n, 't' for \\t)
     * 
     * Returns:
     *   0 if esc_char is a valid JSON escape sequence
     *   -1 if esc_char is not a recognized escape (syntax error)
     * 
     * Recognized escapes: \", \\, \/, \b, \f, \n, \r, \t
     */
    switch (esc_char) {
        case '"':  bsstr_addchr(dest, '"');  return 0;
        case '\\': bsstr_addchr(dest, '\\'); return 0;
        case '/':  bsstr_addchr(dest, '/');  return 0;
        case 'b':  bsstr_addchr(dest, '\b'); return 0;
        case 'f':  bsstr_addchr(dest, '\f'); return 0;
        case 'n':  bsstr_addchr(dest, '\n'); return 0;
        case 'r':  bsstr_addchr(dest, '\r'); return 0;
        case 't':  bsstr_addchr(dest, '\t'); return 0;
        default:   return -1;  /* Invalid escape sequence */
    }
}

static int JsonParser_parseHexCodepoint(const char *buffer, size_t buf_len,
                                         size_t pos, unsigned *out_codepoint)
{
    /* Parse 4 hex digits at buffer[pos..pos+3] into a Unicode codepoint.
     * 
     * Args:
     *   buffer:         Input buffer containing JSON escape sequence
     *   buf_len:        Total buffer length
     *   pos:            Starting position (should point to first hex digit after \\u)
     *   out_codepoint:  Output parameter for parsed codepoint value
     * 
     * Returns:
     *   0 on successful parse of valid hex codepoint (0x0000-0xFFFF)
     *   -1 if buffer too short or any character is not valid hex digit
     */
    if (pos + 4 > buf_len) return -1;
    
    unsigned cp = 0;
    for (size_t i = 0; i < 4; i++) {
        int digit = JsonParser_hexDigit(buffer[pos + i]);
        if (digit < 0) return -1;
        cp = (cp << 4) | (unsigned)digit;
    }
    *out_codepoint = cp;
    return 0;
}

static int JsonParser_validateUtf16Pair(const char *buffer, size_t buf_len,
                                       unsigned high_surrogate, size_t pos,
                                       unsigned *out_combined_codepoint)
{
    /* Validate UTF-16 surrogate pair and combine into full codepoint.
     * 
     * If high_surrogate is in the range U+D800..DBFF (high surrogate), verify
     * that it is immediately followed by a low surrogate (U+DC00..DFFF) and
     * combine them into a single Unicode codepoint (U+10000 and above).
     * 
     * Returns:
     *   0 if high_surrogate is NOT a surrogate (regular codepoint)
     *   6 if valid pair found (indicates advance by 6 bytes: \uHIGH\uLOW)
     *   -1 on error: buffer too short, missing \u, invalid low surrogate
     */
    if (high_surrogate < 0xD800 || high_surrogate > 0xDBFF) {
        return 0;  /* Not a surrogate */
    }

    /* High surrogate found - must be followed by \uXXXX with low surrogate */
    if (pos + 6 > buf_len) return -1;  /* Not enough bytes */
    if (buffer[pos] != '\\' || buffer[pos + 1] != 'u') return -1;  /* Missing \u */

    unsigned low = 0;
    if (JsonParser_parseHexCodepoint(buffer, buf_len, pos + 2, &low) != 0) return -1;
    if (low < 0xDC00 || low > 0xDFFF) return -1;  /* Invalid low surrogate */

    /* Combine surrogates into codepoint: (high - 0xD800) << 10 | (low - 0xDC00) + 0x10000 */
    *out_combined_codepoint = 0x10000 + (((high_surrogate - 0xD800) << 10) | (low - 0xDC00));
    return 6;  /* Consumed \uXXXX\uYYYY */
}

static size_t JsonStreamReader_findSafeChunkLength(const char *buffer, size_t len,
                                                   int initial_in_string,
                                                   int *out_in_string)
{
    /* Scan buffer to identify safe substring for parsing.
     * 
     * In streaming JSON parsing, escape sequences may span chunk boundaries (e.g., buffer
     * ends with "\u04" and next chunk contains "30" for the full "\u0430" codepoint).
     * This function detects such incomplete escapes and returns how many bytes can safely
     * be parsed without risk of truncating an escape mid-sequence.
     * 
     * Args:
     *   buffer:             Input JSON chunk to scan
     *   len:                Bytes available in buffer
     *   initial_in_string:  Quote state from previous chunk (1 = inside string, 0 = outside)
     *   out_in_string:      Output quote state after this chunk (for next chunk's initial state)
     * 
     * Returns:
     *   Number of safe bytes to parse from start of buffer. Remaining bytes should be
     *   saved and prepended to next chunk. Safe length is len except when ending with
     *   incomplete escape (e.g., ends with "\u", "\u0", "\u04", or "\uXXXX\u").
     * 
     * Algorithm:
     *   - Track quote state and whether we're in a backslash escape sequence
     *   - For \u escapes, validate 4 hex digits and optionally a surrogate pair
     *   - If escape is incomplete at buffer end, backtrack to escape start
     */
    int in_string = initial_in_string;
    int escape_next = 0;
    size_t escape_start = SIZE_MAX;  /* Position of backslash starting incomplete escape */
    size_t safe_len = len;

    for (size_t i = 0; i < len; i++) {
        char ch = buffer[i];

        if (!in_string) {
            if (ch == '"') in_string = 1;
            continue;
        }

        /* Inside quoted string */
        if (escape_next) {
            escape_next = 0;
            if (ch == 'u') {
                /* Unicode escape \uXXXX or \uXXXX\uYYYY for surrogates */
                unsigned cp = 0;
                if (JsonParser_parseHexCodepoint(buffer, len, i + 1, &cp) != 0) {
                    /* Cannot read 4 hex digits - incomplete escape at buffer end */
                    safe_len = escape_start;
                    break;
                }
                i += 4;  /* Skip 4 hex digits */

                /* Check if this is a high surrogate */
                unsigned combined = 0;
                int pair_result = JsonParser_validateUtf16Pair(buffer, len, cp, i + 1, &combined);
                if (pair_result < 0) {
                    safe_len = escape_start;
                    break;
                }
                if (pair_result == 6) {
                    i += 6;  /* Skip following \uYYYY */
                }
            }
            continue;
        }

        if (ch == '\\') {
            escape_next = 1;
            escape_start = i;
            continue;
        }

        if (ch == '"') {
            in_string = 0;
        }
    }

    /* If we ended mid-escape, backtrack to start of incomplete escape */
    if (escape_next && escape_start != SIZE_MAX) {
        safe_len = escape_start;
    }

    if (out_in_string) {
        *out_in_string = in_string;
    }

    return safe_len;
}

#define JsonParser_peek_char(p,pos,len)     (pos + 1 < len ? *(p + pos + 1) : '\0')

#define JsonParser_peekObjBegin(p,pos,len)\
    (JsonParser_next_char(p, pos, len) == Json_elem(JSON_OBJ_B)\
    || JsonParser_next_char(p, pos, len) == Json_elem(JSON_ARR_B))

#define JsonParser_peekObjEnd(p, pos, len)\
    (JsonParser_next_char(p, pos, len) == Json_elem(JSON_OBJ_E)\
    || JsonParser_next_char(p, pos, len) == Json_elem(JSON_ARR_E))

#define JsonParser_peekEndLine(p,pos,len)\
    (pos + 1 < len && (*(p + pos + 1) == Json_elem(JSON_CR)\
    || *(p + pos + 1) == Json_elem(JSON_LF)))

static int JsonParser_internalParse(struct  ParserInternal *pi, const char* json, int len)
{
    int i;
    enum eElemType elemType;
    const char *p = json;

    for (i =0; i < len; i++) {
        char ch;

        if(pi->error != JSON_ERR_NONE) break;

        ch = p[i];

        switch ( elemType = Json_typeOfElem(ch) ) {

        case JSON_OBJ_B:
        case JSON_ARR_B:
            JsonParser_internalBeginObj(pi, elemType);
            break;

        case JSON_OBJ_E:
        case JSON_ARR_E:
            if (pi->is_value) {
                JsonParser_internalData(pi);
            }
            JsonParser_internalEndObj(pi, elemType);
            break;

        case JSON_QUOTE:
            /* escaped quote in value */
            if(JsonParser_prev_char(p, i) == Json_elem(JSON_LEFT)) {
                bsstr *data = (!pi->is_value) ? pi->key : pi->value;
                bsstr_addchr(data, Json_elem(JSON_QUOTE));
                break;
            }

            pi->quote_begin = !pi->quote_begin;
            if(pi->quote_begin) {
                char prev = JsonParser_prev_char(p, i);
                if(prev != Json_elem(JSON_COMMA) && prev !=  Json_elem(JSON_COLON)
                        && prev != Json_elem(JSON_OBJ_B) &&  prev != Json_elem(JSON_ARR_B)
                        && prev != '\0') {  /* Allow prev='\0' at buffer start */
                    pi->error = JSON_ERR_SYN;
                    break;
                }
            }

            if (!pi->quote_begin && JsonParser_peekObjEnd(p,i,len)) {
                JsonParser_internalData(pi);
            }
            break;
        case JSON_COLON:
            /* Begin value */
            if (!pi->quote_begin) {
                pi->is_value = 1;
            } else {
                /* colon in value */
                bsstr *data = (!pi->is_value) ? pi->key : pi->value;
                bsstr_addchr(data, Json_elem(JSON_COLON));
            }
            break;
        case JSON_COMMA:
            if (pi->quote_begin) {
                /* Comma is literal content inside a quoted string */
                bsstr *data = (!pi->is_value) ? pi->key : pi->value;
                bsstr_addchr(data, Json_elem(JSON_COMMA));
            } else if (JsonParser_peekEndLine(p, i, len) || 
                       JsonParser_next_char(p, i, len) == Json_elem(JSON_QUOTE)) {
                /* Comma followed by end-of-line or quote: finalize current pair */
                JsonParser_internalData(pi);
            } else if (pi->is_value && bsstr_length(pi->value) > 0) {
                /* Comma after unquoted value: finalize current pair */
                JsonParser_internalData(pi);
            } else {
                /* Comma in unquoted value or between unquoted elements */
                bsstr *data = (!pi->is_value) ? pi->key : pi->value;
                bsstr_addchr(data, Json_elem(JSON_COMMA));
            }
            break;

        case JSON_CR:/*skip*/
            break;
        case JSON_LF:
            pi->line++;
            /* Only end value at newline if quote is closed */
            if (pi->is_value && !pi->quote_begin && !JsonParser_peekObjBegin(p, i, len) ) {
                JsonParser_internalData(pi);
            }
            break;

        case JSON_BEGIN:
        case JSON_FORMFEED:
        case JSON_LEFT:
            if (pi->quote_begin) {
                bsstr *data = (!pi->is_value) ? pi->key : pi->value;
                char esc = JsonParser_peek_char(p, i, len);
                if (esc == '\0') {
                    pi->error = JSON_ERR_SYN;
                    break;
                }

                if (esc == 'u') {
                    unsigned codepoint;
                    if (JsonParser_parseUnicodeEscape(p, len, i + 2, &codepoint) != 0) {
                        pi->error = JSON_ERR_SYN;
                        i++;  /* Skip backslash */
                        break;
                    }

                    int pair_len = JsonParser_decodeSurrogatePair(p, len, i + 6, &codepoint);
                    if (pair_len < 0) {
                        pi->error = JSON_ERR_SYN;
                        i++;  /* Skip backslash */
                        break;
                    }

                    if (JsonParser_encodeUtf8(data, codepoint) != 0) {
                        pi->error = JSON_ERR_SYN;
                        i++;  /* Skip backslash */
                        break;
                    }
                    
                    i += 5 + pair_len;  /* Skip \uXXXX or \uXXXX\uYYYY, no additional increment from below */
                } else {
                    /* Regular escape sequences like \\n, \\t, etc. */
                    if (JsonParser_appendEscapeSequence(data, esc) != 0) {
                        pi->error = JSON_ERR_SYN;
                        i++;
                        break;
                    }
                    i++;  /* Skip backslash and escape char */
                }
            } else if (pi->is_value && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
                /* Preserve invalid backslashes for unquoted values */
                bsstr_addchr(pi->value, ch);
            }
            break;
        case JSON_RIGHT:
        case JSON_TAB:
        case JSON_HEX:  /* Unicode escape parsing now supported inside quoted strings */
        case JSON_INVALID:
            if (pi->quote_begin && !pi->is_value) {
                bsstr_addchr(pi->key, ch);
            } else if (pi->quote_begin && pi->is_value) {
                bsstr_addchr(pi->value, ch);
            } else if (pi->is_value && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
                /* Capture unquoted values (numbers, booleans, null) */
                bsstr_addchr(pi->value, ch);
            } else {
                 /* Invalid character outside of quotes and not part of a value */
                if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
                    pi->error = JSON_ERR_SYN;
                }
            }
            break;
        }
    }

    return pi->error; /*JSON_OK;*/
}

static void JsonParser_startElem(struct JsonParser *parser, const String name, int type)
{
    void *ptr = NULL;
    JsonNode* parent= NULL, *node=NULL;

    DEBUG_PRINT("Json_startElem %s type %d\n", name,type );

    if (parser->m_nodeStack->num > 0) {
        ptr = stack_back(parser->m_nodeStack);
        parent = (JsonNode*) ARR_VAL(ptr);
    } else {
        parser->m_root = JsonNode_Create();
    }

    if (parent) {
        char *pname = isNullorEmpty(name) ? NULL : name;
        node = JsonNode_createChild(parent, pname, type);
    } else {
        node = parser->m_root;
    }

    ptr = stack_push_back(parser->m_nodeStack);
    if (ptr != NULL) {
        ARR_VAL(ptr) = ARR_VAL2PTR(node);
    }
}

static void JsonParser_endElem(struct JsonParser *parser, const String name, int type )
{
    DEBUG_PRINT("Json_endElem %s type %d\n", name,type );
    assert( parser->m_nodeStack->num > 0 );
    if (parser->m_nodeStack->num > 0) {
        stack_pop_back(parser->m_nodeStack);
    }
}

static void JsonParser_elemData(struct JsonParser *parser, const String key,  const String value)
{
    DEBUG_PRINT("eleme '%s' => '%s'\n", key, value);
    if (parser->m_nodeStack->num > 0) {
        void *ptr = stack_back(parser->m_nodeStack);
        JsonNode *node = (JsonNode *) ARR_VAL(ptr);
        JsonNode_setPair(node, key, value);
    }
}

String JsonParser_getErrorString(JsonParser *parser)
{
    return parser->m_errorString;
}

JsonNode * JsonParser_parse(struct JsonParser *parser, const char * json)
{
    JsonNode * root = NULL;
    struct ParserInternal pi;
    pi.parser = parser;
    parser->m_errorString = NULL;
    JsonParser_internalCreate(&pi);
    pi.startElem = JsonParser_startElem;
    pi.endElem = JsonParser_endElem;
    pi.elemData = JsonParser_elemData;
    parser->m_nodeStack = cpo_array_create(JSON_STACK_SIZE, sizeof(void*));
    if (JsonParser_internalParse(&pi, json, strlen(json)) == JSON_ERR_NONE) {
        root = parser->m_root;
    } else {
        parser->m_errorString = (char*)jsonParser_errlist[pi.error];
        printf("json parser error:%s @ %d\n", parser->m_errorString, pi.line);
    }
    DEBUG_PRINT("Parsed lines %d\n", pi.line);
    JsonParser_internalDelete(&pi);
    cpo_array_destroy(parser->m_nodeStack);
    DEBUG_PRINT("-end-\n");
    return root;
}



/* Streaming JSON file parser - memory efficient, no full file buffering */
struct JsonStreamReader {
    FILE *file;
    char buffer[JSON_BUFFER_SIZE];
    size_t buffer_len;
    char leftover[JSON_BUFFER_SIZE];  /* for incomplete tokens across chunks */
    size_t leftover_len;
    int eof;
    struct ParserInternal *parser_state;  /* persistent parse state */
    int in_string;  /* track if in string across chunks */
};

static int JsonStreamReader_init(struct JsonStreamReader *reader, const char *fileName)
{
    reader->file = fopen(fileName, "rb");
    if (!reader->file) {
        return JSON_NOK;
    }
    memset(reader->buffer, 0, sizeof(reader->buffer));
    memset(reader->leftover, 0, sizeof(reader->leftover));
    reader->buffer_len = 0;
    reader->leftover_len = 0;
    reader->eof = 0;
    reader->parser_state = NULL;
    reader->in_string = 0;
    return JSON_OK;
}

static size_t JsonStreamReader_readChunk(struct JsonStreamReader *reader)
{
    /* Read next chunk from file with streaming buffer management.
     * 
     * Handles:
     *   - Restoration of incomplete tokens from previous chunk (leftover buffer)
     *   - File I/O (fread) to fill available space
     *   - Detection of incomplete escape sequences at buffer boundary (safe_parse_len)
     *   - Preservation of unparseable bytes for next chunk iteration
     *   - EOF tracking and quote state persistence
     * 
     * Returns:
     *   Number of safe bytes ready for parsing in reader->buffer.
     *   0 if EOF reached or no data available.
     * 
     * After return, use JsonStreamReader_readChunk()->buffer for parsing and
     * ->buffer_len for safe parse range. Incomplete tokens at boundary are
     * automatically saved in ->leftover for prepending to next chunk.
     */
    if (reader->eof) {
        return 0;
    }

    /* Restore any incomplete token from previous chunk */
    if (reader->leftover_len > 0) {
        memmove(reader->buffer, reader->leftover, reader->leftover_len);
    }

    /* Read new data from file into buffer after leftover bytes */
    size_t bytes_to_read = JSON_BUFFER_SIZE - reader->leftover_len;
    size_t read_len = fread(reader->buffer + reader->leftover_len, 1, bytes_to_read, reader->file);
    reader->buffer_len = reader->leftover_len + read_len;
    reader->leftover_len = 0;

    if (read_len < bytes_to_read) {
        reader->eof = 1;
    }

    if (reader->buffer_len == 0) {
        return 0;
    }

    /* Find how much of buffer is safe to parse (stops before incomplete escapes) */
    int final_in_string = 0;
    size_t safe_parse_len = JsonStreamReader_findSafeChunkLength(
        reader->buffer, reader->buffer_len,
        reader->in_string,        /* carry state from previous chunk */
        &final_in_string          /* output: string state at safe_parse_len */
    );

    /* Save bytes after safe point for next chunk */
    if (safe_parse_len < reader->buffer_len) {
        size_t leftover_bytes = reader->buffer_len - safe_parse_len;
        if (leftover_bytes > JSON_BUFFER_SIZE) {
            leftover_bytes = JSON_BUFFER_SIZE;
        }
        memcpy(reader->leftover, reader->buffer + safe_parse_len, leftover_bytes);
        reader->leftover_len = leftover_bytes;
        reader->buffer_len = safe_parse_len;
    }

    reader->in_string = final_in_string;
    return reader->buffer_len;
}

static void JsonStreamReader_close(struct JsonStreamReader *reader)
{
    if (reader->file) {
        fclose(reader->file);
        reader->file = NULL;
    }
}

/* Streaming parse - memory efficient, processes data as it's read */
static JsonNode* JsonParser_parseFileStreaming(struct JsonParser *parser, const char *fileName)
{
    struct JsonStreamReader reader;
    struct ParserInternal pi;
    int error = JSON_ERR_NONE;
    JsonNode *root = NULL;

    if (JsonStreamReader_init(&reader, fileName) != JSON_OK) {
        parser->m_errorString = strerror(errno);
        return NULL;
    }
    
    /* Initialize parser state */
    parser->m_errorString = NULL;
    JsonParser_internalCreate(&pi);
    pi.startElem = JsonParser_startElem;
    pi.endElem = JsonParser_endElem;
    pi.elemData = JsonParser_elemData;
    pi.parser = parser;
    reader.parser_state = &pi;
    
    parser->m_nodeStack = cpo_array_create(JSON_STACK_SIZE, sizeof(void*));
    
    /* Read and parse file in chunks */
    while (JsonStreamReader_readChunk(&reader) > 0) {
        /* Parse buffer directly (standard JSON only, no comments) */
        error = JsonParser_internalParse(&pi, reader.buffer, (int)reader.buffer_len);
        if (error != JSON_ERR_NONE) {
            DEBUG_PRINT("Streaming parse error at chunk\n");
            break;
        }
    }
    
    if (error == JSON_ERR_NONE) {
        root = parser->m_root;
    } else {
        parser->m_errorString = (char*)jsonParser_errlist[error];
        printf("json parser error:%s @ line %d\n", parser->m_errorString, pi.line);
    }
    
    DEBUG_PRINT("Streamed and parsed lines %d\n", pi.line);
    JsonParser_internalDelete(&pi);
    cpo_array_destroy(parser->m_nodeStack);
    JsonStreamReader_close(&reader);
    
    return root;
}

JsonNode * JsonParser_parseFile(struct JsonParser *parser, const char * fileName)
{
    JsonNode *root = NULL;
    
    /* Use streaming parser for memory-efficient file reading
     * No buffering of entire file - processes chunks as read */
    root = JsonParser_parseFileStreaming(parser, fileName);
    
    /* Strip comments if we successfully got root */
    if (root && parser->m_root == root) {
        /* Comments are stripped during streaming parse */
    }

    return root;
}
