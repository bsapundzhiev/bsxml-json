CC=gcc
CFLAGS=-c -g -Wall 
LDFLAGS=-lexpat -L .
SOURCES=array.c bsxml.c bsjson.c bsstr.c test.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bsxml

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@ 

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	
clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
	
format:
	astyle --style=stroustrup -s4 $(SOURCES)
