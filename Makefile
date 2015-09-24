USE_EXPAT=1
CC=gcc
CFLAGS=-c -g -Wall 
LDFLAGS=-lexpat -L .
SOURCES=array.c bsxml.c bsjson.c bsstr.c test.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bsxml-json-test
OUTPUTFILE=libbsxmljson.a

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@ 

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	
static: all
	ar ru $(OUTPUTFILE) $(OBJECTS)
	ranlib $(OUTPUTFILE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(OUTPUTFILE)
	
format:
	astyle --style=stroustrup -s4 $(SOURCES)
arduino:
	mkdir -p bsjson
	cp -p array.c array.h bsstr.c bsstr.h bsjson.c bsjson.h bsjson.ino bsjson
