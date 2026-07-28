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
#include <limits.h>
#include "bsstr.h"
#include "bsjson.h"

#define JSON_STACK_SIZE 32


/* Check if character is valid for unquoted identifier (JSON5 object keys) */
static int Json5_isIdentifierChar(char ch, int is_first)
{
    if (is_first) {
        return (ch >= 'a' && ch <= 'z') ||
               (ch >= 'A' && ch <= 'Z') ||
               ch == '_' || ch == '$';
    }
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '$';
}

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

static int Json_isStrictNumber(const char *value)
{
    const char *p = value;
    if (*p == '-') p++;
    if (*p == '0') {
        p++;
    } else {
        if (*p < '1' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p == '.') {
        p++;
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') p++;
    }
    return *p == '\0';
}

static int Json_isBareValueValid(const char *value, int json5_enabled)
{
    char *end;
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0
        || strcmp(value, "null") == 0) return 1;
    if (!json5_enabled) return Json_isStrictNumber(value);
    if (strcmp(value, "Infinity") == 0 || strcmp(value, "+Infinity") == 0
        || strcmp(value, "-Infinity") == 0 || strcmp(value, "NaN") == 0
        || strcmp(value, "+NaN") == 0 || strcmp(value, "-NaN") == 0) return 1;
    strtod(value, &end);
    return end != value && *end == '\0';
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
    JsonPair p = { key, NULL, JSON_VALUE_NULL };
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
    JsonNode tmpNode = { type, name, NULL, NULL, NULL };
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

struct ParserFrame {
    char *name;
    enum eElemType opening_type;
};

struct  ParserInternal { /*Jsonlexer*/
    int error;
    int line;
    int quote_begin;
    int is_value;
    int value_was_quoted;
    int token_present;
    bsstr *key;
    bsstr *value;
    cpo_array_t stack;
    struct JsonParser *parser;
    void (*startElem)(struct JsonParser *, const String, int);
    void (*endElem)(struct JsonParser *, const String, int);
    void (*elemData)(struct JsonParser *, const String,  const String);
    /* JSON5 runtime flags */
    int json5_enabled;      /* Enable JSON5 parsing mode */
    char quote_char;        /* Track quote type: '"' or '\'' */
    int in_block_comment;
    int in_line_comment;
    int block_prev_star;
    int pending_slash;
    int after_comma;
    int just_closed_container;
};

static void JsonParser_internalCreate(struct ParserInternal *pi)
{
    pi->error = JSON_ERR_NONE;
    pi->line = 0;
    pi->quote_begin = pi->is_value = pi->value_was_quoted = pi->token_present = 0;
    pi->key = bsstr_create("");
    pi->value = bsstr_create("");
    pi->stack.v = calloc(JSON_STACK_SIZE, sizeof(struct ParserFrame));
    pi->stack.num = 0;
    pi->stack.max = JSON_STACK_SIZE;
    pi->stack.elem_size = sizeof(struct ParserFrame);
    pi->startElem = NULL;
    pi->endElem = NULL;
    pi->elemData = NULL;
    pi->json5_enabled = 0;
    pi->quote_char = '"';
    pi->in_block_comment = 0;
    pi->in_line_comment = 0;
    pi->block_prev_star = 0;
    pi->pending_slash = 0;
    pi->after_comma = 0;
    pi->just_closed_container = 0;
}

static void JsonParser_internalReset(struct ParserInternal *pi)
{
    pi->quote_begin = pi->is_value = pi->value_was_quoted = pi->token_present = 0;
    bsstr_clear(pi->key);
    bsstr_clear(pi->value);
    pi->quote_char = '"';
}

static void JsonParser_internalDelete(struct ParserInternal *pi)
{
    while (pi->stack.num > 0) {
        struct ParserFrame *frame = stack_pop_back(&pi->stack);
        free(frame->name);
    }
    bsstr_delete(pi->key);
    bsstr_delete(pi->value);
    free(pi->stack.v);
}

/* Set runtime JSON5 flag on parser internal state */
static void JsonParser_internalSetJSON5(struct ParserInternal *pi, int enabled)
{
    pi->json5_enabled = enabled ? 1 : 0;
}

static int JsonParser_internalBeginObj(struct  ParserInternal *pi, enum eElemType elemType)
{
    char *name = bsstr_get_buf(pi->key);
    struct ParserFrame *frame;
    if (elemType != JSON_ARR_B && elemType != JSON_OBJ_B) {
        free(name);
        pi->error = JSNON_ERR_NOTOBJ;
        return JSON_NOK;
    }
    frame = stack_push_back(&pi->stack);
    if (!frame) {
        free(name);
        pi->error = JSON_ERR_SYN;
        return JSON_NOK;
    }
    frame->name = name;
    frame->opening_type = elemType;
    if (pi->startElem) {
        pi->startElem(pi->parser, name, (elemType == JSON_ARR_B) ? JSON_ARRAY : JSON_OBJ);
    }

    JsonParser_internalReset(pi);
    return JSON_OK;
}

static int JsonParser_internalEndObj(struct  ParserInternal *pi, enum eElemType elemType)
{
    struct ParserFrame *frame;
    char *name;

    if (pi->stack.num == 0 || (elemType != JSON_ARR_E && elemType != JSON_OBJ_E)) {
        pi->error = JSNON_ERR_NOTOBJ;
        return JSON_NOK;
    }
    frame = stack_back(&pi->stack);
    if ((frame->opening_type == JSON_OBJ_B && elemType != JSON_OBJ_E)
        || (frame->opening_type == JSON_ARR_B && elemType != JSON_ARR_E)) {
        pi->error = JSNON_ERR_NOTOBJ;
        return JSON_NOK;
    }
    frame = stack_pop_back(&pi->stack);
    if ((name = frame->name)) {
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

    if (pi->is_value && !pi->value_was_quoted && bsstr_length(pi->value)
        && !Json_isBareValueValid(bsstr_get_bufref(pi->value), pi->json5_enabled)) {
        pi->error = JSON_ERR_SYN;
        return JSON_NOK;
    }

    if (pi->token_present
        || (pi->is_value ? bsstr_length(pi->value) : bsstr_length(pi->key))) {

        if (pi->elemData) {
            pi->elemData(pi->parser,  bsstr_get_bufref(pi->key), bsstr_get_bufref(pi->value) );
        }
    }

    JsonParser_internalReset(pi);
    return 0;
}

/* Find previous non-whitespace character within buffer bounds
 * Returns '\0' if reached buffer start (don't go past 0) */
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


#define JsonParser_peek_char(p,pos,len)     (pos + 1 < len ? *(p + pos + 1) : '\0')

static int JsonParser_handleQuote(struct ParserInternal *pi, char ch)
{
    bsstr *data;

    if (ch != '"' && !(pi->json5_enabled && ch == '\'')) return 0;
    if (pi->quote_begin) {
        if (ch == pi->quote_char) {
            pi->quote_begin = 0;
            pi->quote_char = '"';
        } else {
            data = pi->is_value ? pi->value : pi->key;
            bsstr_addchr(data, ch);
        }
        return 1;
    }
    if (pi->token_present
        || (pi->is_value ? bsstr_length(pi->value) : bsstr_length(pi->key))) {
        pi->error = JSON_ERR_SYN;
        return 1;
    }
    pi->quote_begin = 1;
    pi->quote_char = ch;
    pi->token_present = 1;
    pi->after_comma = 0;
    pi->just_closed_container = 0;
    if (pi->is_value) pi->value_was_quoted = 1;
    return 1;
}

static int JsonParser_internalParse(struct  ParserInternal *pi, const char* json, int len)
{
    int i = 0;
    enum eElemType elemType;
    const char *p = json;
    /* Parsing errors are stored in pi->error and reported by the public API. */

    for (; i < len; i++) {
        char ch;

        if(pi->error != JSON_ERR_NONE) break;

        ch = p[i];

        if (pi->in_line_comment) {
            if (ch == '\n') {
                pi->in_line_comment = 0;
                pi->line++;
            }
            continue;
        }
        if (pi->in_block_comment) {
            if (pi->block_prev_star && ch == '/') {
                pi->in_block_comment = 0;
                pi->block_prev_star = 0;
                continue;
            }
            pi->block_prev_star = (ch == '*');
            if (ch == '\n') pi->line++;
            continue;
        }
        if (pi->pending_slash) {
            pi->pending_slash = 0;
            if (ch == '/') {
                pi->in_line_comment = 1;
                continue;
            }
            if (ch == '*') {
                pi->in_block_comment = 1;
                pi->block_prev_star = 0;
                continue;
            }
            pi->error = JSON_ERR_SYN;
            break;
        }

        /* Skip comments when JSON5 runtime mode enabled */
        if (pi->json5_enabled && !pi->quote_begin && ch == '/') {
            if (i + 1 < len) {
                if (p[i + 1] == '/') {
                    /* Skip line comment */
                    i++;
                    while (i < len && p[i] != '\n') i++;
                    if (i >= len) {
                        pi->in_line_comment = 1;
                        return JSON_ERR_NONE;
                    }
                    pi->line++;
                    continue;
                } else if (p[i + 1] == '*') {
                    /* Skip block comment */
                    i += 2;
                    int comment_closed = 0;
                    while (i + 1 < len) {
                        if (p[i] == '*' && p[i + 1] == '/') {
                            i++;
                            comment_closed = 1;
                            break;
                        }
                        if (p[i] == '\n') pi->line++;
                        i++;
                    }
                    if (!comment_closed) {
                        pi->in_block_comment = 1;
                        pi->block_prev_star = (len > 0 && p[len - 1] == '*');
                        return JSON_ERR_NONE;
                    }
                    continue;
                }
            } else {
                pi->pending_slash = 1;
                return JSON_ERR_NONE;
            }
        }

        if (JsonParser_handleQuote(pi, ch)) continue;

        if (!pi->quote_begin && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n'
            && ch != ',' && ch != '}' && ch != ']') {
            pi->after_comma = 0;
            pi->just_closed_container = 0;
        }

        switch ( elemType = Json_typeOfElem(ch) ) {

        case JSON_OBJ_B:
        case JSON_ARR_B:
            pi->after_comma = 0;
            pi->just_closed_container = 0;
            JsonParser_internalBeginObj(pi, elemType);
            break;

        case JSON_OBJ_E:
        case JSON_ARR_E:
            if (pi->after_comma && !pi->json5_enabled) {
                pi->error = JSON_ERR_COMMA;
                break;
            }
            pi->after_comma = 0;
            if (pi->token_present || bsstr_length(pi->key) || bsstr_length(pi->value)) {
                JsonParser_internalData(pi);
            }
            JsonParser_internalEndObj(pi, elemType);
            pi->just_closed_container = 1;
            break;

        case JSON_QUOTE:
            pi->error = JSON_ERR_SYN; /* Quotes are handled before the switch. */
            break;
        case JSON_COLON:
            /* Begin value */
            if (!pi->quote_begin) {
                if (pi->is_value || (!pi->token_present && bsstr_length(pi->key) == 0)) {
                    pi->error = JSON_ERR_SYN;
                    break;
                }
                pi->is_value = 1;
                pi->token_present = 0;
                pi->value_was_quoted = 0;
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
            } else if (pi->token_present || bsstr_length(pi->key) || bsstr_length(pi->value)) {
                JsonParser_internalData(pi);
                pi->after_comma = 1;
            } else if (pi->just_closed_container) {
                pi->after_comma = 1;
                pi->just_closed_container = 0;
            } else {
                pi->error = JSON_ERR_COMMA;
            }
            break;

        case JSON_CR:/*skip*/
            break;
        case JSON_LF:
            pi->line++;
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
                    if (pi->json5_enabled && pi->quote_char == '\'' && esc == '\'') {
                        bsstr_addchr(data, '\'');
                    } else if (JsonParser_appendEscapeSequence(data, esc) != 0) {
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
                pi->token_present = 1;
                bsstr_addchr(pi->value, ch);
            } else if (pi->json5_enabled
                       && Json5_isIdentifierChar(ch, bsstr_length(pi->key) == 0)) {
                /* JSON5 unquoted object key (ASCII identifier subset). */
                pi->token_present = 1;
                bsstr_addchr(pi->key, ch);
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
    (void)name;
    (void)type;
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

static int JsonParser_internalFinish(struct ParserInternal *pi)
{
    if (pi->error != JSON_ERR_NONE) return pi->error;
    if (pi->quote_begin || pi->in_block_comment || pi->pending_slash
        || pi->stack.num != 0) {
        pi->error = pi->quote_begin ? JSON_ERR_QUOTE : JSON_ERR_SYN;
    }
    return pi->error;
}

static JsonNode *JsonParser_parseMode(JsonParser *parser, const char *json, int json5_enabled)
{
    JsonNode * root = NULL;
    struct ParserInternal pi;
    int error;

    if (!parser || !json) return NULL;
    pi.parser = parser;
    parser->m_errorString = NULL;
    parser->m_root = NULL;
    JsonParser_internalCreate(&pi);
    JsonParser_internalSetJSON5(&pi, json5_enabled);
    pi.startElem = JsonParser_startElem;
    pi.endElem = JsonParser_endElem;
    pi.elemData = JsonParser_elemData;
    parser->m_nodeStack = cpo_array_create(JSON_STACK_SIZE, sizeof(void*));
    error = JsonParser_internalParse(&pi, json, (int)strlen(json));
    if (error == JSON_ERR_NONE) error = JsonParser_internalFinish(&pi);
    if (error == JSON_ERR_NONE) {
        root = parser->m_root;
    } else {
        parser->m_errorString = (char*)jsonParser_errlist[error];
        printf("json parser error:%s @ %d\n", parser->m_errorString, pi.line);
        if (parser->m_root) {
            JsonNode_deleteTree(parser->m_root);
            parser->m_root = NULL;
        }
    }
    DEBUG_PRINT("Parsed lines %d\n", pi.line);
    JsonParser_internalDelete(&pi);
    cpo_array_destroy(parser->m_nodeStack);
    DEBUG_PRINT("-end-\n");
    return root;
}

JsonNode *JsonParser_parse(JsonParser *parser, const char *json)
{
    return JsonParser_parseMode(parser, json, 0);
}

JsonNode *JsonParser_parseJSON5(JsonParser *parser, const char *json)
{
    return JsonParser_parseMode(parser, json, 1);
}

static int JsonParser_isActiveBackslash(const char *buffer, size_t pos)
{
    size_t preceding = 0;
    while (pos > preceding && buffer[pos - preceding - 1] == '\\') preceding++;
    return (preceding % 2) == 0;
}

static size_t JsonParser_safeChunkLength(const char *buffer, size_t len)
{
    size_t start = len > 12 ? len - 12 : 0;

    for (size_t pos = start; pos < len; ++pos) {
        unsigned codepoint;
        size_t remaining;
        if (buffer[pos] != '\\' || !JsonParser_isActiveBackslash(buffer, pos)) continue;
        remaining = len - pos;
        if (remaining == 1) return pos;
        if (buffer[pos + 1] != 'u') continue;
        if (remaining < 6) return pos;
        if (JsonParser_parseUnicodeEscape(buffer, (int)len, (int)pos + 2, &codepoint) != 0) {
            continue; /* Complete but invalid escapes are reported by the parser. */
        }
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF && remaining < 12) {
            size_t pair_remaining = remaining - 6;
            if (pair_remaining == 0 || (buffer[pos + 6] == '\\'
                && (pair_remaining == 1 || buffer[pos + 7] == 'u'))) {
                return pos;
            }
        }
    }
    if (len > 0 && buffer[len - 1] == '/') return len - 1;
    return len;
}

static JsonNode *JsonParser_parseFileMode(JsonParser *parser, const char *fileName,
                                          int json5_enabled, size_t chunk_size)
{
    FILE *file;
    char *buffer;
    size_t leftover_len = 0;
    int error = JSON_ERR_NONE;
    int eof = 0;
    JsonNode *root = NULL;
    struct ParserInternal pi;

    if (!parser || !fileName) return NULL;
    if (chunk_size < 12 || chunk_size > (size_t)INT_MAX - 12) {
        parser->m_errorString = "Chunk size must be between 12 and INT_MAX - 12 bytes";
        return NULL;
    }
    file = fopen(fileName, "rb");
    if (!file) {
        parser->m_errorString = strerror(errno);
        return NULL;
    }

    buffer = malloc(chunk_size + 12);
    if (!buffer) {
        parser->m_errorString = strerror(errno);
        fclose(file);
        return NULL;
    }

    pi.parser = parser;
    parser->m_errorString = NULL;
    parser->m_root = NULL;
    JsonParser_internalCreate(&pi);
    JsonParser_internalSetJSON5(&pi, json5_enabled);
    pi.startElem = JsonParser_startElem;
    pi.endElem = JsonParser_endElem;
    pi.elemData = JsonParser_elemData;
    parser->m_nodeStack = cpo_array_create(JSON_STACK_SIZE, sizeof(void*));

    while (!eof) {
        size_t read_len = fread(buffer + leftover_len, 1, chunk_size, file);
        size_t buffer_len = leftover_len + read_len;
        size_t safe_len;
        eof = read_len < chunk_size;
        if (ferror(file)) {
            parser->m_errorString = strerror(errno);
            error = JSON_ERR_SYN;
            break;
        }
        safe_len = eof ? buffer_len : JsonParser_safeChunkLength(buffer, buffer_len);
        if (safe_len > 0) {
            error = JsonParser_internalParse(&pi, buffer, (int)safe_len);
            if (error != JSON_ERR_NONE) break;
        }
        leftover_len = buffer_len - safe_len;
        if (leftover_len > 0) memmove(buffer, buffer + safe_len, leftover_len);
    }

    if (error == JSON_ERR_NONE) error = JsonParser_internalFinish(&pi);
    if (error == JSON_ERR_NONE) {
        root = parser->m_root;
    } else {
        if (!parser->m_errorString) parser->m_errorString = (char*)jsonParser_errlist[error];
        if (parser->m_root) {
            JsonNode_deleteTree(parser->m_root);
            parser->m_root = NULL;
        }
    }

    JsonParser_internalDelete(&pi);
    cpo_array_destroy(parser->m_nodeStack);
    fclose(file);
    free(buffer);
    return root;
}

JsonNode *JsonParser_parseFile(JsonParser *parser, const char *fileName)
{
    return JsonParser_parseFileMode(parser, fileName, 0, JSON_BUFFER_SIZE);
}

JsonNode *JsonParser_parseFileWithChunkSize(JsonParser *parser, const char *fileName,
                                            size_t chunk_size)
{
    return JsonParser_parseFileMode(parser, fileName, 0, chunk_size);
}

JsonNode *JsonParser_parseFileJSON5(JsonParser *parser, const char *fileName)
{
    return JsonParser_parseFileMode(parser, fileName, 1, JSON_BUFFER_SIZE);
}

JsonNode *JsonParser_parseFileJSON5WithChunkSize(JsonParser *parser, const char *fileName,
                                                 size_t chunk_size)
{
    return JsonParser_parseFileMode(parser, fileName, 1, chunk_size);
}
