//
//Arduino bsjson example
//
namespace bs {
  extern "C" {
    #include "bsjson.h"
    String JsonNode_getJSON_NR(JsonNode *root);
  }
}

using  namespace bs;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  mkJson();
  delay(1000);
}

//Test 
void printJson( bs::JsonNode *node )
{
  Serial.write("printJson:\n");
  bs::String str = JsonNode_getJSON_NR(node);
  Serial.write(str);
  free(str);
}

void mkJson() 
{
    JsonNode *root = JsonNode_Create();
    JsonNode *address , *phone, *phoneType;
  
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
}

