USE_EXPAT=1
CC=gcc
CFLAGS=-c -g -Wall 
LDFLAGS=-lexpat -L .
SOURCES=array.c bsxml.c bsjson.c bsstr.c mem.c test.c
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
