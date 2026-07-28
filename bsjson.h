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

#ifndef __BSJSON_H
#define __BSJSON_H

#include <stddef.h>
#include "array.h"

#ifdef _WIN32
#define strdup _strdup
#endif

#ifndef JSON_BUFFER_SIZE
/* Default file-parser read size; explicit WithChunkSize APIs override it per call. */
#define JSON_BUFFER_SIZE 32
#endif

//#define DEBUG_JSON
#ifdef DEBUG_JSON
#define DEBUG_PRINT printf
#else
#define DEBUG_PRINT(...)
#endif

enum {JSON_NOK, JSON_OK };
enum {JSON_NONE, JSON_ROOT, JSON_OBJ, JSON_ARRAY };
enum {JSON_VALUE_STRING = 0, JSON_VALUE_NUMBER, JSON_VALUE_BOOL, JSON_VALUE_NULL};

#define NAME_ANON NULL

#define JSON_IS_OBJ(node)\
    (node->m_type == JSON_OBJ || node->m_type == JSON_ROOT)
#define JSON_IS_ARRAY(node)\
    (node->m_type == JSON_ARRAY)

typedef char * String;
typedef struct JsonNode JsonNode;
typedef struct JsonParser JsonParser;
typedef struct JsonPair JsonPair;

struct JsonPair {
    String key;
    String value;
    int type;  /* JSON_VALUE_STRING, JSON_VALUE_NUMBER, JSON_VALUE_BOOL, JSON_VALUE_NULL */
};

struct JsonNode {
    int m_type;
    String m_name;
    JsonNode *m_parent;
    cpo_array_t *m_pairs; /* array of JsonPair */
    cpo_array_t *m_childs; /* array of JsonNode */
};

struct JsonParser {
    JsonNode *m_root;
    cpo_array_t *m_nodeStack;
    String m_errorString;
};

/* Parser entry points */
JsonNode *JsonParser_parse(JsonParser *parser, const char * json);
JsonNode *JsonParser_parseFile(JsonParser *parser, const char *fileName);
JsonNode *JsonParser_parseFileWithChunkSize(JsonParser *parser, const char *fileName,
                                            size_t chunk_size);
JsonNode *JsonParser_parseJSON5(JsonParser *parser, const char *json);
JsonNode *JsonParser_parseFileJSON5(JsonParser *parser, const char *fileName);
JsonNode *JsonParser_parseFileJSON5WithChunkSize(JsonParser *parser, const char *fileName,
                                                 size_t chunk_size);
String JsonParser_getErrorString(JsonParser *parser);

/* Node helpers */
JsonNode *JsonNode_Create();
JsonNode *JsonNode_createChild(JsonNode *node, String name, int type);
JsonNode *JsonNode_createObject(JsonNode * node, String name);
JsonNode *JsonNode_createArray(JsonNode * node, String name);
JsonNode * JsonNode_findChild(JsonNode *node, const String name, int type);
JsonPair * JsonNode_findPair(JsonNode *node, const String key);
void JsonNode_setPair(JsonNode *node, const String key, const String value );
asize_t JsonNode_getChildCount(JsonNode * node);
asize_t JsonNode_getPairCount(JsonNode *node);
JsonNode * JsonNode_getChild(JsonNode *node, asize_t index);
JsonPair * JsonNode_getPair(JsonNode *node, asize_t index);
String JsonNode_getPairValue(JsonNode *node, const String key);
int JsonNode_getPairValueInt(JsonNode *node, const String key);
double JsonNode_getPairValueDouble(JsonNode *node, const String key);
int JsonNode_getPairValueType(JsonNode *node, const String key);
void JsonNode_delete(JsonNode *node);
void JsonNode_deleteTree(JsonNode *root);
String JsonNode_getJSON(JsonNode *node);

#endif //__BSJSON_H
