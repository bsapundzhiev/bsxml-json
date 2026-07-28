#if (_WIN32)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "bsxml.h"
#include "bsjson.h"

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else
#include <sys/time.h>
#endif
#ifdef _WIN32

HANDLE hConsole;
CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
WORD saved_attributes;

static const unsigned __int64 epoch = (__int64)(116444736000000000);
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    FILETIME    file_time;
    SYSTEMTIME  system_time;
    ULARGE_INTEGER ularge;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    ularge.LowPart = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;

    tp->tv_sec = (long) ((ularge.QuadPart - epoch) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);

    return 0;
}

void set_color()
{
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    saved_attributes = consoleInfo.wAttributes;
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
}
void restore_color()
{
    SetConsoleTextAttribute(hConsole, saved_attributes);
}
#else
void set_color()
{
    printf("\033[32m");
}
void restore_color()
{
    printf("\033[0m");
}
#endif

struct ctmr {
    struct timeval start, end;
};

void clock_init(struct ctmr *t)
{
    memset(&(t)->start, 0, sizeof(struct timeval));
    memset(&(t)->end, 0, sizeof(struct timeval));
}

void clock_on(struct ctmr *t)
{
    gettimeofday(&(t)->start, NULL);
}

long clock_off(struct ctmr *t)
{
    long secs, usecs;
    gettimeofday(&(t)->end, NULL);
    secs  = t->end.tv_sec  - t->start.tv_sec;
    usecs = t->end.tv_usec - t->start.tv_usec;
    return (long)(((secs) * 1000 + usecs/1000.0) + 0.5);
}

struct ctmr t;

#define CLK_ON(x) \
	clock_init(x); \
	clock_on(x); \

#define CLK_OFF(x)\
	set_color();\
	printf("[%s] end in %ld ms\n",__FUNCTION__, clock_off(x));\
	restore_color();\

static const char xml[] = "<?xml version=\"1.0\"?>\n\
<A:propfind xmlns:A=\"DAV:\">\n\
    <A:prop name=\"test\">\n\
        <A:displayname/>\n\
        <A:resourcetype/>\n\
        <A:getcontenttype/>\n\
        <A:getcontentlength/>\n\
        <A:getlastmodified/>\n\
        <A:lockdiscovery/>\n\
        <A:checked-in/>\n\
        <A:checked-out/>\n\
        <A:version-name/>\n\
    </A:prop>\n\
</A:propfind>\n";

void printXml( XmlNodeRef node )
{
    String str = XmlNode_getXML(node);
    printf("%s", str);
    free(str);
}

void create_test ()
{
    XmlNodeRef root, child1, child2, child3;
    CLK_ON(&t);
    root   = XmlNode_Create( "ROOT" );
    child1 = XmlNode_createChild(root, 	"FirstChild", NULL);
    child2 = XmlNode_createChild(root,	"SecondChild", NULL);
    child3 = XmlNode_createChild(child2, "ThirdChild", "This is text node");

    XmlNode_setAttribute(child1, "name", "First" );
    XmlNode_setAttribute(child2, "name", "Second" );
    XmlNode_setAttribute(child3, "name", "Third" );

    printXml(root);
    XmlNode_deleteTree(root);
    CLK_OFF(&t);
}

void find_test()
{
    XmlParser xmlParser;
    XmlNodeRef root = XmlParser_parse(&xmlParser,  xml );
    CLK_ON(&t);
    if (root) {
        asize_t i;

        for (i = 0; i < XmlNode_getChildCount(root); i++) {

            XmlNodeRef child = XmlNode_getChild(root, i);
            if (XmlNode_isTag(child, "A:prop")) {

                char *attr = XmlNode_getAttributeValue(child, "name");
                printf("found attr %s -> %s\n", "name", attr);

                if (attr && !strcmp ( attr, "test") ) {
                    printf("found \n");
                }
            }
        }
    }

    XmlNode_deleteTree(root);
    CLK_OFF(&t);
}

void file_test(int argc, char **argv)
{
    String param = "test/test3.xml";
    XmlNodeRef root;
    XmlParser xmlParser;

    if (argc > 1) {
        param = argv[argc-1];
    }
    CLK_ON(&t);
    printf("parse file %s\n", param);
    root = XmlParser_parse_file(&xmlParser, param);

    if (root) {
        printXml( root );
    } else {
        printf("Err: %s\n", XmlParser_getErrorString(&xmlParser));
    }

    XmlNode_deleteTree(root);
    CLK_OFF(&t);
}

void printJson( JsonNode *node )
{
    String str = JsonNode_getJSON(node);
    printf("%s", str);
    free(str);
}

int json_parser_test(String param, int json5_enabled)
{
    JsonParser parser;
    JsonNode *root;
    int result = 0;
    CLK_ON(&t);
    printf("parse file %s\n", param);
    root = json5_enabled
        ? JsonParser_parseFileJSON5(&parser, param)
        : JsonParser_parseFile(&parser, param);
    if (root) {
        String serialized = JsonNode_getJSON(root);
        printf("%s", serialized);
        if (strcmp(param, "test/test2.json") == 0
            && (strstr(serialized, "C,Python") != NULL
                || strstr(serialized, "\",projects") != NULL)) {
            fprintf(stderr, "file parser corrupted a token boundary\n");
            result = -1;
        }
        free(serialized);
    } else {
        printf("Err: %s\n", JsonParser_getErrorString(&parser));
        result = -1;
    }

    JsonNode_deleteTree(root);
    CLK_OFF(&t);
    return result;
}

static int json5_regression_test(void)
{
    JsonParser parser;
    JsonNode *root;
    int result = 0;
    const char *invalid_json[] = {
        "{\"unterminated\": \"value}",
        "{\"missing_end\": true",
        "{\"mismatched\": ]}",
        "{\"comment\": true // not JSON\n}"
    };

    root = JsonParser_parseJSON5(&parser,
        "{/* comment */ unquoted: 'it\\'s valid', hex: 0x2a, value: NaN,}");
    if (!root
        || !JsonNode_getPairValue(root, "unquoted")
        || strcmp(JsonNode_getPairValue(root, "unquoted"), "it's valid") != 0
        || !JsonNode_getPairValue(root, "hex")
        || strcmp(JsonNode_getPairValue(root, "hex"), "0x2a") != 0
        || !JsonNode_getPairValue(root, "value")
        || strcmp(JsonNode_getPairValue(root, "value"), "NaN") != 0) {
        fprintf(stderr, "JSON5 regression test failed\n");
        result = -1;
    }
    JsonNode_deleteTree(root);

    root = JsonParser_parseJSON5(&parser, "{/* unterminated");
    if (root != NULL) {
        fprintf(stderr, "unterminated JSON5 comment was accepted\n");
        JsonNode_deleteTree(root);
        result = -1;
    }

    root = JsonParser_parse(&parser, "{'not': 'json'}");
    if (root != NULL) {
        fprintf(stderr, "single quotes were accepted in strict JSON mode\n");
        JsonNode_deleteTree(root);
        result = -1;
    }

    root = JsonParser_parse(&parser, "{\"trailing\": true,}");
    if (root != NULL) {
        fprintf(stderr, "trailing comma was accepted in strict JSON mode\n");
        JsonNode_deleteTree(root);
        result = -1;
    }
    for (size_t i = 0; i < sizeof(invalid_json) / sizeof(invalid_json[0]); ++i) {
        root = JsonParser_parse(&parser, invalid_json[i]);
        if (root != NULL) {
            fprintf(stderr, "invalid JSON input was accepted: %s\n", invalid_json[i]);
            JsonNode_deleteTree(root);
            result = -1;
        }
    }
    return result;
}

static int streaming_chunk_test(const char *file_name, int json5_enabled)
{
    JsonParser parser;
    JsonNode *root;
    String expected;
    int result = 0;

    root = json5_enabled
        ? JsonParser_parseFileJSON5WithChunkSize(&parser, file_name, 128)
        : JsonParser_parseFileWithChunkSize(&parser, file_name, 128);
    if (!root) return -1;
    expected = JsonNode_getJSON(root);
    JsonNode_deleteTree(root);

    for (size_t chunk_size = 12; chunk_size <= 48; ++chunk_size) {
        String actual;
        root = json5_enabled
            ? JsonParser_parseFileJSON5WithChunkSize(&parser, file_name, chunk_size)
            : JsonParser_parseFileWithChunkSize(&parser, file_name, chunk_size);
        if (!root) {
            fprintf(stderr, "stream parse failed for %s with chunk %zu: %s\n",
                    file_name, chunk_size, JsonParser_getErrorString(&parser));
            result = -1;
            continue;
        }
        actual = JsonNode_getJSON(root);
        if (strcmp(expected, actual) != 0) {
            fprintf(stderr, "stream output differs for %s with chunk %zu\n",
                    file_name, chunk_size);
            result = -1;
        }
        free(actual);
        JsonNode_deleteTree(root);
    }
    free(expected);
    return result;
}

void json_create_test ()
{
    JsonNode *root = JsonNode_Create();
    JsonNode *address, *phone, *phoneType;
    CLK_ON(&t);
    JsonNode_setPair(root, "firstName", "John" );
    JsonNode_setPair(root, "lastName", "Smith" );
    JsonNode_setPair(root, "age", "25" );

    address = JsonNode_createObject(root, "address");
    JsonNode_setPair(address, "streetAddress", "21 2nd Street");
    JsonNode_setPair(address, "city", "New York");
    JsonNode_setPair(address, "state", "NY");
    JsonNode_setPair(address, "postalCode", "10021");

    phone = JsonNode_createArray(root, "phoneNumber");
    phoneType = JsonNode_createObject(phone, NAME_ANON);
    JsonNode_setPair(phoneType, "type", "home");
    JsonNode_setPair(phoneType, "number", "212 555-1234");

    phoneType = JsonNode_createObject(phone, NAME_ANON);
    JsonNode_setPair(phoneType, "type", "fax");
    JsonNode_setPair(phoneType, "number", "646 555-4567");

    printJson( root );
    JsonNode_deleteTree(root);
    CLK_OFF(&t);
}

#ifdef _ARRAY_TEST
/* d */
void cpo_array_dump_int(cpo_array_t *arr)
{
    asize_t i = 0;
    for (i = 0; i < arr->num; i++) {
        void* x =  cpo_array_get_at(arr, i);
        printf("[%lu] %d\n",i, *((int*)x) );
    }
}

void cpo_array_dump_str(cpo_array_t *arr)
{
    asize_t i = 0;
    for (i = 0; i < arr->num; i++) {
        char *x = cpo_array_get_at(arr, i);
        printf("[%lu] %s\n",i, x);
    }
}

void array_test()
{
    int i;
    void *x;
    cpo_array_t arr;

    arr.elem_size = sizeof(int);
    arr.v = calloc(1, sizeof(int));
    arr.num = 0;
    arr.max = 1;

    for (i=0; i< 10; i++) {

        x = cpo_array_push(&arr);
        //x = stack_push(&arr);
        *((int*)x) = i;
    }

    cpo_array_dump_int(&arr);

    for (i=0; i< 10; i++) {
        x = stack_pop_back(&arr);
        printf("pop[%d] %d\n", i, *((int*)x) );
    }
    //printf("ins at %d num %d\n", i, arr.num);
    //x = cpo_array_insert_at(&arr, 6);
    //*((int*)x) = 5000;

    cpo_array_dump_int(&arr);
    free(arr.v);
}
#endif

int main(int argc, char **argv)
{
#ifdef _ARRAY_TEST
    array_test();
#else
    int json_result = 0;
    file_test(argc,argv);
    create_test ();
    find_test();
    /*test json */
    json_create_test();
    json_result |= json_parser_test("test/test2.json", 0);
    json_result |= json_parser_test("test/unicode_escapes.json", 0);
    json_result |= json_parser_test("test/unicode_test.json", 0);
    json_result |= json_parser_test("test/nested.json", 0);
    json_result |= json_parser_test("test/test_json5_example.json5", 1);
    json_result |= json5_regression_test();
    json_result |= streaming_chunk_test("test/test2.json", 0);
    json_result |= streaming_chunk_test("test/unicode_test.json", 0);
    json_result |= streaming_chunk_test("test/test_json5_example.json5", 1);
    if (json_result != 0) {
        return 1;
    }
#endif
#ifdef _WIN32
    _CrtDumpMemoryLeaks();
    system("pause");
#endif
    return 0;
}
