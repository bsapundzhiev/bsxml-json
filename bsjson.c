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

enum JsonGrammarState {
    JSON_GRAMMAR_OBJECT_KEY_OR_END,
    JSON_GRAMMAR_OBJECT_VALUE,
    JSON_GRAMMAR_OBJECT_COMMA_OR_END,
    JSON_GRAMMAR_ARRAY_VALUE_OR_END,
    JSON_GRAMMAR_ARRAY_COMMA_OR_END
};

struct ParserFrame {
    char *name;
    enum eElemType opening_type;
    enum JsonGrammarState grammar_state;
    int after_comma;
};

enum JsonTokenState {
    JSON_TOKEN_NONE,
    JSON_TOKEN_BARE,
    JSON_TOKEN_BARE_ENDED,
    JSON_TOKEN_STRING
};

struct JsonLexer {
    int in_string;
    enum JsonTokenState token_state;
    char quote_char;
    int in_block_comment;
    int in_line_comment;
    int block_prev_star;
    int pending_slash;
};

struct ParserInternal {
    int error;
    int line;
    struct JsonLexer lexer;
    bsstr *key;
    bsstr *value;
    cpo_array_t stack;
    struct JsonParser *parser;
    void (*startElem)(struct JsonParser *, const String, int);
    void (*endElem)(struct JsonParser *, const String, int);
    void (*elemData)(struct JsonParser *, const String,  const String);
    /* JSON5 runtime flags */
    int json5_enabled;      /* Enable JSON5 parsing mode */
    int root_started;
    int root_complete;
};

static struct ParserFrame *JsonParser_currentFrame(struct ParserInternal *pi)
{
    if (!pi || pi->stack.num == 0) return NULL;
    return stack_back(&pi->stack);
}

static int JsonParser_expectsObjectValue(struct ParserInternal *pi)
{
    struct ParserFrame *frame = JsonParser_currentFrame(pi);
    return frame && frame->opening_type == JSON_OBJ_B
        && frame->grammar_state == JSON_GRAMMAR_OBJECT_VALUE;
}

static bsstr *JsonParser_tokenBuffer(struct ParserInternal *pi)
{
    return JsonParser_expectsObjectValue(pi) ? pi->value : pi->key;
}

static void JsonLexer_init(struct JsonLexer *lexer)
{
    memset(lexer, 0, sizeof(*lexer));
    lexer->quote_char = '"';
}

static void JsonLexer_resetToken(struct JsonLexer *lexer)
{
    lexer->in_string = 0;
    lexer->token_state = JSON_TOKEN_NONE;
    lexer->quote_char = '"';
}

static int JsonLexer_hasToken(const struct JsonLexer *lexer)
{
    return lexer->token_state != JSON_TOKEN_NONE;
}

static int JsonLexer_tokenIsQuoted(const struct JsonLexer *lexer)
{
    return lexer->token_state == JSON_TOKEN_STRING;
}

static int JsonParser_currentTokenIsValid(struct ParserInternal *pi)
{
    bsstr *token;

    if (JsonLexer_tokenIsQuoted(&pi->lexer)) return 1;

    token = JsonParser_tokenBuffer(pi);
    if (bsstr_length(token) == 0) return 1;

    return Json_isBareValueValid(bsstr_get_bufref(token), pi->json5_enabled);
}

static void JsonParser_internalCreate(struct ParserInternal *pi)
{
    pi->error = JSON_ERR_NONE;
    pi->line = 0;
    JsonLexer_init(&pi->lexer);
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
    pi->root_started = 0;
    pi->root_complete = 0;
}

static void JsonParser_internalReset(struct ParserInternal *pi)
{
    JsonLexer_resetToken(&pi->lexer);
    bsstr_clear(pi->key);
    bsstr_clear(pi->value);
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
    if (pi->stack.num == 0) {
        if (pi->root_started || pi->root_complete) {
            free(name);
            pi->error = JSON_ERR_SYN;
            return JSON_NOK;
        }
        pi->root_started = 1;
    } else {
        struct ParserFrame *parent = JsonParser_currentFrame(pi);
        int expects_value = parent->grammar_state == JSON_GRAMMAR_OBJECT_VALUE
            || parent->grammar_state == JSON_GRAMMAR_ARRAY_VALUE_OR_END;
        if (!expects_value || JsonLexer_hasToken(&pi->lexer)) {
            free(name);
            pi->error = JSON_ERR_SYN;
            return JSON_NOK;
        }
        parent->after_comma = 0;
    }
    frame = stack_push_back(&pi->stack);
    if (!frame) {
        free(name);
        pi->error = JSON_ERR_SYN;
        return JSON_NOK;
    }
    frame->name = name;
    frame->opening_type = elemType;
    frame->grammar_state = elemType == JSON_OBJ_B
        ? JSON_GRAMMAR_OBJECT_KEY_OR_END : JSON_GRAMMAR_ARRAY_VALUE_OR_END;
    frame->after_comma = 0;
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
    frame = JsonParser_currentFrame(pi);
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
    if (pi->stack.num == 0) {
        pi->root_complete = 1;
    } else {
        struct ParserFrame *parent = JsonParser_currentFrame(pi);
        parent->grammar_state = parent->opening_type == JSON_OBJ_B
            ? JSON_GRAMMAR_OBJECT_COMMA_OR_END : JSON_GRAMMAR_ARRAY_COMMA_OR_END;
        parent->after_comma = 0;
    }
    return 0;
}

static int JsonParser_internalData(struct  ParserInternal *pi)
{
    if (pi->lexer.in_string) {
        pi->error = JSON_ERR_QUOTE;
        return JSON_NOK;
    }

    if (!JsonParser_currentTokenIsValid(pi)) {
        pi->error = JSON_ERR_SYN;
        return JSON_NOK;
    }

    if (JsonLexer_hasToken(&pi->lexer) || bsstr_length(JsonParser_tokenBuffer(pi))) {

        if (pi->elemData) {
            pi->elemData(pi->parser,  bsstr_get_bufref(pi->key), bsstr_get_bufref(pi->value) );
        }
    }

    JsonParser_internalReset(pi);
    return 0;
}

static int JsonParser_finishScalar(struct ParserInternal *pi, struct ParserFrame *frame)
{
    if (!frame || (frame->opening_type == JSON_OBJ_B
            && frame->grammar_state != JSON_GRAMMAR_OBJECT_VALUE)
        || (frame->opening_type == JSON_ARR_B
            && frame->grammar_state != JSON_GRAMMAR_ARRAY_VALUE_OR_END)) {
        pi->error = JSON_ERR_SYN;
        return JSON_NOK;
    }
    JsonParser_internalData(pi);
    if (pi->error != JSON_ERR_NONE) {
        return JSON_NOK;
    }
    frame->grammar_state = frame->opening_type == JSON_OBJ_B
        ? JSON_GRAMMAR_OBJECT_COMMA_OR_END : JSON_GRAMMAR_ARRAY_COMMA_OR_END;
    return JSON_OK;
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


static int JsonParser_handleQuote(struct ParserInternal *pi, char ch)
{
    bsstr *data;
    struct ParserFrame *frame;

    if (ch != '"' && !(pi->json5_enabled && ch == '\'')) return 0;
    if (pi->lexer.in_string) {
        if (ch == pi->lexer.quote_char) {
            pi->lexer.in_string = 0;
            pi->lexer.quote_char = '"';
        } else {
            data = JsonParser_tokenBuffer(pi);
            bsstr_addchr(data, ch);
        }
        return 1;
    }
    if (pi->root_complete) {
        pi->error = JSON_ERR_SYN;
        return 1;
    }
    frame = JsonParser_currentFrame(pi);
    if (!frame) {
        pi->error = JSON_ERR_SYN;
        return 1;
    } else {
        int expected = frame->opening_type == JSON_OBJ_B
            ? (frame->grammar_state == JSON_GRAMMAR_OBJECT_VALUE
                || frame->grammar_state == JSON_GRAMMAR_OBJECT_KEY_OR_END)
            : frame->grammar_state == JSON_GRAMMAR_ARRAY_VALUE_OR_END;
        if (!expected) {
            pi->error = JSON_ERR_SYN;
            return 1;
        }
    }
    if (JsonLexer_hasToken(&pi->lexer) || bsstr_length(JsonParser_tokenBuffer(pi))) {
        pi->error = JSON_ERR_SYN;
        return 1;
    }
    pi->lexer.in_string = 1;
    pi->lexer.quote_char = ch;
    pi->lexer.token_state = JSON_TOKEN_STRING;
    frame->after_comma = 0;
    return 1;
}

enum JsonLexerResult {
    JSON_LEXER_NOT_HANDLED = 0,
    JSON_LEXER_CONSUMED = 1,
    JSON_LEXER_ERROR = -1
};

static enum JsonLexerResult JsonLexer_handleEscape(struct ParserInternal *pi,
                                                    const char *buffer,
                                                    int *position,
                                                    int len)
{
    bsstr *data;
    char escape_char;
    unsigned codepoint;
    int pair_len;

    if (buffer[*position] != '\\' || !pi->lexer.in_string) {
        return JSON_LEXER_NOT_HANDLED;
    }
    if (*position + 1 >= len) {
        pi->error = JSON_ERR_SYN;
        return JSON_LEXER_ERROR;
    }
    data = JsonParser_tokenBuffer(pi);
    escape_char = buffer[*position + 1];
    if (escape_char != 'u') {
        if (pi->json5_enabled && pi->lexer.quote_char == '\'' && escape_char == '\'') {
            bsstr_addchr(data, '\'');
        } else if (JsonParser_appendEscapeSequence(data, escape_char) != 0) {
            pi->error = JSON_ERR_SYN;
            return JSON_LEXER_ERROR;
        }
        (*position)++;
        return JSON_LEXER_CONSUMED;
    }
    if (JsonParser_parseUnicodeEscape(buffer, len, *position + 2, &codepoint) != 0) {
        pi->error = JSON_ERR_SYN;
        return JSON_LEXER_ERROR;
    }
    pair_len = JsonParser_decodeSurrogatePair(buffer, len, *position + 6, &codepoint);
    if (pair_len < 0 || JsonParser_encodeUtf8(data, codepoint) != 0) {
        pi->error = JSON_ERR_SYN;
        return JSON_LEXER_ERROR;
    }
    *position += 5 + pair_len;
    return JSON_LEXER_CONSUMED;
}

static enum JsonLexerResult JsonLexer_handleComment(struct ParserInternal *pi,
                                                     const char *buffer,
                                                     int *position,
                                                     int len)
{
    char ch = buffer[*position];

    if (pi->lexer.in_line_comment) {
        if (ch == '\n') {
            pi->lexer.in_line_comment = 0;
            pi->line++;
        }
        return JSON_LEXER_CONSUMED;
    }
    if (pi->lexer.in_block_comment) {
        if (pi->lexer.block_prev_star && ch == '/') {
            pi->lexer.in_block_comment = 0;
            pi->lexer.block_prev_star = 0;
        } else {
            pi->lexer.block_prev_star = (ch == '*');
            if (ch == '\n') pi->line++;
        }
        return JSON_LEXER_CONSUMED;
    }
    if (pi->lexer.pending_slash) {
        pi->lexer.pending_slash = 0;
        if (ch == '/') {
            pi->lexer.in_line_comment = 1;
            return JSON_LEXER_CONSUMED;
        }
        if (ch == '*') {
            pi->lexer.in_block_comment = 1;
            pi->lexer.block_prev_star = 0;
            return JSON_LEXER_CONSUMED;
        }
        pi->error = JSON_ERR_SYN;
        return JSON_LEXER_ERROR;
    }
    if (!pi->json5_enabled || pi->lexer.in_string || ch != '/') {
        return JSON_LEXER_NOT_HANDLED;
    }
    if (*position + 1 >= len) {
        pi->lexer.pending_slash = 1;
        return JSON_LEXER_CONSUMED;
    }
    if (buffer[*position + 1] == '/') {
        pi->lexer.in_line_comment = 1;
        (*position)++;
        return JSON_LEXER_CONSUMED;
    }
    if (buffer[*position + 1] == '*') {
        pi->lexer.in_block_comment = 1;
        pi->lexer.block_prev_star = 0;
        (*position)++;
        return JSON_LEXER_CONSUMED;
    }
    pi->error = JSON_ERR_SYN;
    return JSON_LEXER_ERROR;
}

static int JsonLexer_isStructural(char ch)
{
    return ch == '{' || ch == '}' || ch == '[' || ch == ']'
        || ch == ':' || ch == ',';
}

static int JsonLexer_isWhitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static enum JsonLexerResult JsonLexer_handleText(struct ParserInternal *pi, char ch)
{
    bsstr *data;
    struct ParserFrame *frame;

    if (pi->lexer.in_string) {
        if ((unsigned char)ch < 0x20) {
            pi->error = JSON_ERR_SYN;
            return JSON_LEXER_ERROR;
        }
        data = JsonParser_tokenBuffer(pi);
        bsstr_addchr(data, ch);
        return JSON_LEXER_CONSUMED;
    }
    if (JsonLexer_isWhitespace(ch)) {
        if (ch == '\n') pi->line++;
        if (pi->lexer.token_state == JSON_TOKEN_BARE) {
            pi->lexer.token_state = JSON_TOKEN_BARE_ENDED;
        }
        return JSON_LEXER_CONSUMED;
    }
    if (JsonLexer_isStructural(ch)) {
        return JSON_LEXER_NOT_HANDLED;
    }
    frame = JsonParser_currentFrame(pi);
    if (pi->root_complete || !frame) {
        pi->error = JSON_ERR_SYN;
        return JSON_LEXER_ERROR;
    }
    if (pi->lexer.token_state == JSON_TOKEN_BARE_ENDED) {
        pi->error = JSON_ERR_SYN;
        return JSON_LEXER_ERROR;
    }
    {
        int expected = frame->opening_type == JSON_OBJ_B
            ? (frame->grammar_state == JSON_GRAMMAR_OBJECT_VALUE
                || frame->grammar_state == JSON_GRAMMAR_OBJECT_KEY_OR_END)
            : frame->grammar_state == JSON_GRAMMAR_ARRAY_VALUE_OR_END;
        if (!expected) {
            pi->error = JSON_ERR_SYN;
            return JSON_LEXER_ERROR;
        }
    }
    frame->after_comma = 0;
    if (frame->grammar_state == JSON_GRAMMAR_OBJECT_VALUE
        || frame->opening_type == JSON_ARR_B) {
        pi->lexer.token_state = JSON_TOKEN_BARE;
        bsstr_addchr(JsonParser_tokenBuffer(pi), ch);
        return JSON_LEXER_CONSUMED;
    }
    if (pi->json5_enabled
        && Json5_isIdentifierChar(ch, bsstr_length(pi->key) == 0)) {
        pi->lexer.token_state = JSON_TOKEN_BARE;
        bsstr_addchr(pi->key, ch);
        return JSON_LEXER_CONSUMED;
    }
    pi->error = JSON_ERR_SYN;
    return JSON_LEXER_ERROR;
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

        enum JsonLexerResult comment_result = JsonLexer_handleComment(pi, p, &i, len);
        if (comment_result == JSON_LEXER_CONSUMED) continue;
        if (comment_result == JSON_LEXER_ERROR) break;

        enum JsonLexerResult escape_result = JsonLexer_handleEscape(pi, p, &i, len);
        if (escape_result == JSON_LEXER_CONSUMED) continue;
        if (escape_result == JSON_LEXER_ERROR) break;

        if (JsonParser_handleQuote(pi, ch)) continue;

        enum JsonLexerResult text_result = JsonLexer_handleText(pi, ch);
        if (text_result == JSON_LEXER_CONSUMED) continue;
        if (text_result == JSON_LEXER_ERROR) break;

        switch ( elemType = Json_typeOfElem(ch) ) {

        case JSON_OBJ_B:
        case JSON_ARR_B:
            JsonParser_internalBeginObj(pi, elemType);
            break;

        case JSON_OBJ_E:
        case JSON_ARR_E:
            {
                struct ParserFrame *frame = JsonParser_currentFrame(pi);
                if (!frame) {
                    pi->error = JSNON_ERR_NOTOBJ;
                    break;
                }
                if (frame->opening_type == JSON_OBJ_B
                    && ((frame->grammar_state == JSON_GRAMMAR_OBJECT_VALUE
                         && !JsonLexer_hasToken(&pi->lexer)
                         && bsstr_length(pi->value) == 0)
                        || (frame->grammar_state == JSON_GRAMMAR_OBJECT_KEY_OR_END
                            && (JsonLexer_hasToken(&pi->lexer)
                            || bsstr_length(pi->key) > 0)))) {
                    pi->error = JSON_ERR_SYN;
                    break;
                }
                if (JsonLexer_hasToken(&pi->lexer)
                    || bsstr_length(pi->key) || bsstr_length(pi->value)) {
                    if (JsonParser_finishScalar(pi, frame) != JSON_OK) break;
                }
                if (frame->grammar_state != JSON_GRAMMAR_OBJECT_COMMA_OR_END
                    && frame->grammar_state != JSON_GRAMMAR_ARRAY_COMMA_OR_END
                    && !((frame->grammar_state == JSON_GRAMMAR_OBJECT_KEY_OR_END
                          || frame->grammar_state == JSON_GRAMMAR_ARRAY_VALUE_OR_END)
                         && (!frame->after_comma || pi->json5_enabled))) {
                    pi->error = JSON_ERR_SYN;
                    break;
                }
                if (frame->after_comma && !pi->json5_enabled) {
                    pi->error = JSON_ERR_COMMA;
                    break;
                }
            }
            JsonParser_internalEndObj(pi, elemType);
            break;

        case JSON_QUOTE:
            pi->error = JSON_ERR_SYN; /* Quotes are handled before the switch. */
            break;
        case JSON_COLON:
            /* Begin value */
            if (!pi->lexer.in_string) {
                struct ParserFrame *frame = JsonParser_currentFrame(pi);
                if (!frame || frame->opening_type != JSON_OBJ_B
                    || frame->grammar_state != JSON_GRAMMAR_OBJECT_KEY_OR_END
                    || (!JsonLexer_hasToken(&pi->lexer) && bsstr_length(pi->key) == 0)) {
                    pi->error = JSON_ERR_SYN;
                    break;
                }
                frame->grammar_state = JSON_GRAMMAR_OBJECT_VALUE;
                JsonLexer_resetToken(&pi->lexer);
            }
            break;
        case JSON_COMMA:
            if (pi->stack.num == 0) {
                pi->error = JSON_ERR_COMMA;
            } else if (JsonLexer_hasToken(&pi->lexer)
                       || bsstr_length(pi->key) || bsstr_length(pi->value)) {
                struct ParserFrame *frame = JsonParser_currentFrame(pi);
                if (JsonParser_finishScalar(pi, frame) != JSON_OK) break;
                frame->grammar_state = frame->opening_type == JSON_OBJ_B
                    ? JSON_GRAMMAR_OBJECT_KEY_OR_END : JSON_GRAMMAR_ARRAY_VALUE_OR_END;
                frame->after_comma = 1;
            } else {
                struct ParserFrame *frame = JsonParser_currentFrame(pi);
                if (frame->grammar_state != JSON_GRAMMAR_OBJECT_COMMA_OR_END
                    && frame->grammar_state != JSON_GRAMMAR_ARRAY_COMMA_OR_END) {
                    pi->error = JSON_ERR_COMMA;
                    break;
                }
                frame->grammar_state = frame->opening_type == JSON_OBJ_B
                    ? JSON_GRAMMAR_OBJECT_KEY_OR_END : JSON_GRAMMAR_ARRAY_VALUE_OR_END;
                frame->after_comma = 1;
            }
            break;

        case JSON_CR:
        case JSON_LF:
        case JSON_BEGIN:
        case JSON_FORMFEED:
        case JSON_LEFT:
        case JSON_RIGHT:
        case JSON_TAB:
        case JSON_HEX:
        case JSON_INVALID:
            pi->error = JSON_ERR_SYN; /* Text is handled before the switch. */
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
    if (pi->lexer.in_string || pi->lexer.in_block_comment || pi->lexer.pending_slash
        || pi->stack.num != 0 || !pi->root_complete) {
        pi->error = pi->lexer.in_string ? JSON_ERR_QUOTE : JSON_ERR_SYN;
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
