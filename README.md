# Fairly simple and portable XML DOM and JSON implementation in C

You can do both parsing and creating DOM and JSON type objects.

## Build

**NOTE:** XML Parser is wrapper around expat library.  
`USE_EXPAT=1` in Makefile comment it out for none(only json).

Build as static library with 'gnu make':

```bash
$ sudo apt-get install libexpat1-dev
$ make lib
```

## JSON parsing

Standard JSON and the supported JSON5 subset have separate entry points:

```c
JsonParser parser;

JsonNode *strict = JsonParser_parse(&parser, json_text);
JsonNode *json5 = JsonParser_parseJSON5(&parser, json5_text);
```

File input is parsed incrementally. The default file APIs use `JSON_BUFFER_SIZE`, which is
32 bytes unless overridden at compile time:

```c
JsonNode *strict_file = JsonParser_parseFile(&parser, "data.json");
JsonNode *json5_file = JsonParser_parseFileJSON5(&parser, "data.json5");
```

Resource-constrained callers can select the input chunk size per call:

```c
JsonNode *root = JsonParser_parseFileWithChunkSize(&parser, "data.json", 2048);
JsonNode *root5 = JsonParser_parseFileJSON5WithChunkSize(
    &parser, "data.json5", 2048);
```

The minimum supported chunk size is 12 bytes. Input buffering uses at most
`chunk_size + 12` bytes. Total parsing memory is approximately
`O(chunk size + longest token + parsed tree)` because a token and the resulting DOM still
need to be stored independently of the input buffer size.

## License

See COPYING file for copying permission.
