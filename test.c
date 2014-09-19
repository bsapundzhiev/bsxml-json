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

void set_color() {
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    saved_attributes = consoleInfo.wAttributes;
    SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
}
void restore_color() {
    SetConsoleTextAttribute(hConsole, saved_attributes);
}
#else
void set_color() { printf( "\033"); }
void restore_color() { printf("\033"); }
#endif

struct ctmr {
	struct timeval start, end;
};

void clock_init(struct ctmr *t) {
	memset(&(t)->start, 0, sizeof(struct timeval));
	memset(&(t)->end, 0, sizeof(struct timeval));
}

void clock_on(struct ctmr *t) {
	gettimeofday(&(t)->start, NULL);
}

long clock_off(struct ctmr *t) {
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
    <A:prop>\n\
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
	int i;
	XmlParser xmlParser;
	XmlNodeRef root = XmlParser_parse(&xmlParser,  xml );
	CLK_ON(&t);
	if (root) {
	
		for (i = 0; i < XmlNode_getChildCount(root); i++) {
		
				XmlNodeRef child = XmlNode_getChild(root, i);
				if (XmlNode_isTag(child, "A:prop")) {
				
					char *attr = XmlNode_getAttribute(child, "name");
					printf("found %s\n", "A:prop");
					
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
	
	if(argc > 1) {
		param = argv[argc-1];
		printf("parse file %s\n", param);
	}
	CLK_ON(&t);
	root = XmlParser_parse_file(&xmlParser, param);
	
	if (root) {
		printXml( root );
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
	JsonParser parser;
	JsonNode *root;
	CLK_ON(&t);
	
	//root = JsonParser_parse(&parser, json2);
	//root = JsonParser_parse(&parser, json);
	root = JsonParser_parseFile(&parser, "test/test2.json");
	if (root) {
		printJson( root );
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

int main(int argc, char **argv) 
{
	file_test(argc,argv);
	create_test ();
	find_test();
	/*test json */
	json_create_test();
	json_parser_test();

#ifdef _WIN32
	 _CrtDumpMemoryLeaks();
	 system("pause");
#endif
	return 0;
}
