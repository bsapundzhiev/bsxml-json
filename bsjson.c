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
#include "bsjson.h"

#define JSON_STACK_SIZE 32

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
    return	jsonElems[type];
}

static enum eElemType Json_typeOfElem(const char c)
{
    enum eElemType type = JSON_INVALID;
    for (type = JSON_OBJ_B; type != JSON_INVALID; ++type) {
        if (c == jsonElems[type]) break;
    }

    return type;
}

JsonNode * JsonNode_Create()
{
    JsonNode *node = (JsonNode *) malloc( sizeof(JsonNode) );
    if (!node) return NULL;

    node->m_type = JSON_ROOT;
    node->m_name = NULL;
    node->m_parent = NULL;
    node->m_pairs = cpo_array_create(4 , sizeof(JsonPair));
    node->m_childs =  cpo_array_create(4 , sizeof(JsonNode));
    return node;
}

JsonNode * JsonNode_createChild(JsonNode * node, String name, int type)
{
    JsonNode * child = (JsonNode *)cpo_array_push(node->m_childs);
    child->m_type = type;
    child->m_parent = node;
    child->m_name = (name != NULL) ? strdup(name) : NULL;
    child->m_pairs = cpo_array_create(4 , sizeof(JsonPair));
    child->m_childs =  cpo_array_create(4 , sizeof(JsonNode));
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

void JsonNode_delete(JsonNode *node)
{
    int i;
    if (!node) return;
    for (i=0; i < node->m_pairs->num; i++) {
        JsonPair *pair = (JsonPair*)cpo_array_get_at(node->m_pairs, i);
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
    int i;
    if (!root) return;
    for (i=0 ; i < root->m_childs->num; i++) {
        JsonNode *node = (JsonPair*)cpo_array_get_at(root->m_childs, i);
        JsonNode_deleteTree(node);
    }

    JsonNode_delete(root);

    if (root->m_type == JSON_ROOT) {
        free(root);
        root = NULL;
    }
}

String JsonNode_getJSON(JsonNode *node)
{
    int i,nPairs, nChilds;
    String JSON = NULL;
    bsstr *buff = bsstr_create("");

    if (!isNullorEmpty(node->m_name)) {
        bsstr_printf(buff, "\"%s\":", node->m_name);
    }

    bsstr_printf(buff, "%s\n",  JSON_IS_OBJ(node) ? "{" : "[");
    nPairs = node->m_pairs->num;
    nChilds = node->m_childs->num;

    for (i=0; i < nPairs; i++ ) {
        JsonPair *pair = (JsonPair*)cpo_array_get_at(node->m_pairs, i);

        if (JSON_IS_ARRAY(node)) {
            bsstr_printf(buff, "\"%s\"", pair->key);
        } else {
            bsstr_printf(buff, "\"%s\":\"%s\"", pair->key, pair->value);
        }

        bsstr_printf(buff, "%s\n", (i < nPairs -1 || nChilds > 0) ? "," : "");
    }

    for (i = 0; i < nChilds; i++) {
        JsonNode* child = (JsonNode*)cpo_array_get_at(node->m_childs, i);
        String childJSON = JsonNode_getJSON(child);
        bsstr_add(buff, childJSON);
        if (i < nChilds -1) {
            int len = bsstr_lenght(buff)-1;
            strncpy(bsstr_get_bufref(buff) + len, ",", 1);
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

    if (bsstr_lenght(pi->key) || bsstr_lenght(pi->value)) {

        if (pi->elemData) {
            pi->elemData(pi->parser,  bsstr_get_bufref(pi->key), bsstr_get_bufref(pi->value) );
        }
    }

    JsonParser_internalReset(pi);
    return 0;
}

static __inline char JsonParser_next_char(const char *p , int pos)
{
    char ch;
    while ( (ch = *(p+ ++pos)) != '\0' ) {
        if (ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t' )
            break;
    }

    return ch;
}

static __inline char JsonParser_prev_char(const char *p , int pos)
{
    char ch;
    while ( (ch = *(p+ --pos)) != '\0' ) {
        if (ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t' )
            break;
    }

    return ch;
}

#define JsonParser_peek_char(p,pos)		*(p + pos + 1)

#define JsonParser_peekObjBegin(p,pos)\
	(JsonParser_next_char(p, pos) == Json_elem(JSON_OBJ_B)\
	|| JsonParser_next_char(p, pos) == Json_elem(JSON_ARR_B))

#define JsonParser_peekObjEnd(p, pos)\
	(JsonParser_next_char(p, pos) == Json_elem(JSON_OBJ_E)\
	|| JsonParser_next_char(p, pos) == Json_elem(JSON_ARR_E))

#define JsonParser_peekEndLine(p,pos)\
	(JsonParser_peek_char(p,pos) == Json_elem(JSON_CR)\
	|| JsonParser_peek_char(p, pos) == Json_elem(JSON_LF))

static int JsonParser_internalParse(struct  ParserInternal *pi, const char* json , int len)
{
    int i;
    char ch;
    enum eElemType elemType;
    const char *p = json;

    for (i =0; i < len + 1; i++) {

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
            pi->quote_begin = !pi->quote_begin;

            if( pi->quote_begin) {
                char prev = JsonParser_prev_char(p, i);
                if(prev != Json_elem(JSON_COMMA) && prev !=  Json_elem(JSON_COLON)
                        && prev != Json_elem(JSON_OBJ_B) &&  prev != Json_elem(JSON_ARR_B) ) {
                    pi->error = JSON_ERR_SYN;
                    break;
                }
            }

            if (!pi->quote_begin && JsonParser_peekObjEnd(p,i)) {
                JsonParser_internalData(pi);
            }
            break;
        case JSON_COLON:
            /* Begin value */
            if (!pi->quote_begin) {
                pi->is_value = 1;
            } else {
                /* quote in string */
                bsstr_addchr(pi->key, Json_elem(JSON_COLON));
            }
            break;
        case JSON_COMMA:
            if (JsonParser_peekEndLine(p,i) || JsonParser_next_char(p,i) == Json_elem(JSON_QUOTE)) {
                JsonParser_internalData(pi);
            } else {
                /* comma in string */
                if (pi->is_value) {
                    bsstr_addchr(pi->value, Json_elem(JSON_COMMA));
                } else {
                    /* XXX: comma in key? */
                    pi->error = JSON_ERR_COMMA;
                }
            }
            break;

        case JSON_CR:/*skip*/
            break;
        case JSON_LF:
            pi->line++;
            if (pi->is_value && !JsonParser_peekObjBegin(p, i) ) {
                JsonParser_internalData(pi);
            }
            break;

        case JSON_BEGIN:
        case JSON_FORMFEED:
        case JSON_LEFT:
        case JSON_RIGHT:
        case JSON_TAB:
        case JSON_HEX:	/*TODO:*/
        case JSON_INVALID:
            //printf("%c", ch);
            if (pi->quote_begin && !pi->is_value) {
                bsstr_addchr(pi->key, ch);
            } else if (pi->is_value) {
                bsstr_addchr(pi->value, ch);
            } else {
                //printf("[skiped] '%c' [0x%x]\n", ch,ch);
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

    DEBUG_PRINT("Json_startElem %s type %d\n", name ,type );

    if (parser->m_nodeStack->num > 0) {
        ptr = stack_back(parser->m_nodeStack);
        parent = (JsonNode*) ARR_VAL(ptr);
    } else {
        parser->m_root = JsonNode_Create();
    }

    if (parent) {
        char *pname = isNullorEmpty(name) ? NULL : name;
        node = JsonNode_createChild(parent, pname , type);
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
    DEBUG_PRINT("Json_endElem %s type %d\n", name ,type );
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

JsonNode * JsonParser_parse(struct JsonParser *parser, const char * json)
{
    JsonNode * root = NULL;
    struct ParserInternal pi;
    pi.parser = parser;
    JsonParser_internalCreate(&pi);
    pi.startElem = JsonParser_startElem;
    pi.endElem = JsonParser_endElem;
    pi.elemData = JsonParser_elemData;
    parser->m_nodeStack = cpo_array_create(JSON_STACK_SIZE , sizeof(void*));
    if (JsonParser_internalParse(&pi, json, strlen(json)) == JSON_ERR_NONE) {
        root = parser->m_root;
    } else {
        printf("TODO: parser error:%d @ %d\n", pi.error , pi.line);
    }
    printf("Parsed lines %d\n",  pi.line);
    JsonParser_internalDelete(&pi);
    cpo_array_destroy(parser->m_nodeStack);
    printf("-end-\n");
    return root;
}

JsonNode * JsonParser_parseFile(struct JsonParser *parser, const char * fileName)
{
    char * buffer = 0;
    long length =0;
    JsonNode * root = NULL;
    FILE *f = fopen (fileName, "r");

    if (f) {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = (char*) malloc (length + 1);
        if (buffer) {
            fread (buffer, sizeof(char), length, f);
            buffer[length] = '\0';
        }

        fclose (f);
        root = JsonParser_parse(parser,  buffer);
        free(buffer);
    } else {
        printf("error: cant read %s \n", fileName);
    }

    return root;
}
