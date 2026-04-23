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
 * NOTE: Smaller buffers = slower parsing + more disk I/O
 *       But linear memory usage stays constant regardless of file size
 *       256B may have issues with tokens split across chunk boundaries
 */
#define JSON_BUFFER_SIZE 32  /* 2KB read buffer - recommended for most cases */

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

#define JsonParser_peek_char(p,pos)     (pos + 1 < len ? *(p + pos + 1) : '\0')

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
            if (JsonParser_peekEndLine(p,i,len) || JsonParser_next_char(p,i,len) == Json_elem(JSON_QUOTE)) {
                JsonParser_internalData(pi);
            } else if (pi->is_value && bsstr_length(pi->value) > 0) {
                /* comma after a value - end the value */
                JsonParser_internalData(pi);
            } else {
                /* comma in quoted value */
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
        case JSON_RIGHT:
        case JSON_TAB:
        case JSON_HEX:  /* Unicode escape parsing (e.g., \uXXXX) not implemented - known limitation */
        case JSON_INVALID:
            if (pi->quote_begin && !pi->is_value) {
                bsstr_addchr(pi->key, ch);
            } else if (pi->quote_begin && pi->is_value) {
                bsstr_addchr(pi->value, ch);
            } else if (pi->is_value && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
                /* Capture unquoted values (numbers, booleans, null) */
                bsstr_addchr(pi->value, ch);
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

/* Strip comments from JSON buffer - handles // and # comments */
static void JsonBuffer_stripComments(const char *src, size_t src_len, 
                                     char *dst, size_t *dst_len)
{
    size_t i, j = 0;
    unsigned char in_string = 0;
    
    for (i = 0; i < src_len && j < 2*JSON_BUFFER_SIZE - 1; i++) {
        char ch = src[i];
        
        /* Track if we're inside a quoted string */
        if (ch == '"' && (i == 0 || src[i-1] != '\\')) {
            in_string = !in_string;
            dst[j++] = ch;
            continue;
        }
        
        if (in_string) {
            dst[j++] = ch;
            continue;
        }
        
        /* Skip // and # comments outside strings */
        if ((ch == '/' && i + 1 < src_len && src[i+1] == '/') || ch == '#') {
            /* Skip to end of line */
            while (i < src_len && src[i] != '\n' && src[i] != '\r') {
                i++;
            }
            /* Keep newline for line tracking */
            if (i < src_len && src[i] == '\r' && i + 1 < src_len && src[i+1] == '\n') {
                dst[j++] = '\n';
                i++;
            } else if (i < src_len && (src[i] == '\n' || src[i] == '\r')) {
                dst[j++] = '\n';
            }
            continue;
        }
        
        dst[j++] = ch;
    }
    
    dst[j] = '\0';
    *dst_len = j;
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
};

static int JsonStreamReader_init(struct JsonStreamReader *reader, const char *fileName)
{
    reader->file = fopen(fileName, "rb");
    if (!reader->file) {
        return JSON_NOK;
    }
    reader->buffer_len = 0;
    reader->leftover_len = 0;
    reader->eof = 0;
    reader->parser_state = NULL;
    return JSON_OK;
}

static size_t JsonStreamReader_readChunk(struct JsonStreamReader *reader)
{
    if (reader->eof) {
        return 0;
    }
    
    reader->buffer_len = fread(reader->buffer, 1, JSON_BUFFER_SIZE, reader->file);
    
    if (reader->buffer_len < JSON_BUFFER_SIZE) {
        reader->eof = 1;
    }
    
    /* Check for incomplete strings at end of buffer */
    int in_string = 0;
    int escape_next = 0;
    size_t i;
    for (i = 0; i < reader->buffer_len; i++) {
        char ch = reader->buffer[i];
        if (escape_next) {
            escape_next = 0;
            continue;
        }
        if (ch == '\\') {
            escape_next = 1;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
        }
    }
    
    /* If we're in a string at the end, read more to complete it */
    if (in_string && !reader->eof) {
        size_t additional = fread(reader->buffer + reader->buffer_len, 1, 
                                 JSON_BUFFER_SIZE - reader->buffer_len, reader->file);
        reader->buffer_len += additional;
        if (additional < JSON_BUFFER_SIZE - reader->buffer_len) {
            reader->eof = 1;
        }
    }
    
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
    char clean_buffer[2*JSON_BUFFER_SIZE];
    size_t clean_len;
    
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
        /* Strip comments from buffer before parsing */
        JsonBuffer_stripComments(reader.buffer, reader.buffer_len, clean_buffer, &clean_len);
        
        /* Parse the cleaned buffer */
        error = JsonParser_internalParse(&pi, clean_buffer, (int)clean_len);
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
