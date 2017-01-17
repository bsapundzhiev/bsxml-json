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

static const char json [] =
    "{\n\
    \"firstName\": \"John\",\n\
    \"lastName\": \"Smith\",\n\
    \"age\": 25,\n\
    \"address\": {\n\
        \"streetAddress\": \"21 2nd Street\",\n\
        \"city\": \"New York\",\n\
        \"state\": \"NY\",\n\
        \"postalCode\": \"10021\"\n\
    },\n\
    \"phoneNumber\": [\n\
        {\n\
            \"type\": \"home\",\n\
            \"number\": \"212 555-1234\"\n\
        },\n\
        {\n\
            \"type\": \"fax\",\n\
			\"number\": \"646 555-4567\"\n\
        }\n\
    ]\n\
}\n";

static const char json2 [] =
    "{\n\
	 \"address\": {\n\
        \"streetAddress\": \"21 2nd Street\",\n\
    },\n\
	\"GlossSeeAlso\": [\n\
		{ \"servlet-name\": \"cofaxCDS\",\n\
			\"init-param\": {\n\
        		\"configGlossary:installationAt\": \"Philadelphia, PA\"\n\
        	},\n\
        },\n\
		\"GML\",\n\
		\"XML\"\n\
	]\n\
}\n";

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
    asize_t i;
    XmlParser xmlParser;
    XmlNodeRef root = XmlParser_parse(&xmlParser,  xml );
    CLK_ON(&t);
    if (root) {

        for (i = 0; i < XmlNode_getChildCount(root); i++) {

            XmlNodeRef child = XmlNode_getChild(root, i);
            if (XmlNode_isTag(child, "A:prop")) {

                char *attr = XmlNode_getAttributeValue(child, "name");
                printf("found attr %s -> %s\n", "name", attr);

                if (attr && !strcmp ( attr  , "test") ) {
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

void json_parser_test()
{
    String param = "test/test2.json";
    JsonParser parser;
    JsonNode *root;
    CLK_ON(&t);

    //root = JsonParser_parse(&parser, json2);
    //root = JsonParser_parse(&parser, json);
    printf("parse file %s\n", param);
    root = JsonParser_parseFile(&parser, param);
    if (root) {
        printJson( root );
    } else {
        printf("Err: %s\n", JsonParser_getErrorString(&parser));
    }

    JsonNode_deleteTree(root);
    CLK_OFF(&t);
}

void json_create_test ()
{
    JsonNode *root = JsonNode_Create();
    JsonNode *address , *phone, *phoneType;
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
    void* x;
    for (i = 0; i < arr->num; i++) {
        x =  cpo_array_get_at(arr, i);
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
    file_test(argc,argv);
    create_test ();
    find_test();
    /*test json */
    json_create_test();
    json_parser_test();
#endif
#ifdef _WIN32
    _CrtDumpMemoryLeaks();
    system("pause");
#endif
    return 0;
}
