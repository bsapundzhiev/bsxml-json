#
# Makefile
# 
CC=gcc
CFLAGS=-c -g -Wall -std=gnu99
LDFLAGS=-lexpat -L .
USE_EXPAT=1

SOURCES=array.c bsjson.c bsstr.c test.c
SOURCES_LIB=array.c bsjson.c bsstr.c
ifdef USE_EXPAT
SOURCES +=bsxml.c
SOURCES_LIB +=bsxml.c 
endif

OBJECTS=$(SOURCES:.c=.o)
OBJECTS_LIB=$(SOURCES_LIB:.c=.o)
EXECUTABLE=bsxml-json-test
OUTPUTFILE=libbsxmljson.a

all: $(SOURCES) $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(EXECUTABLE) 

#	$(CC) $(OBJECTS) $(LDFLAGS) -o $@ 

.c.o:
	$(CC) $(CFLAGS) $< -o $@
	

lib: $(SOURCES_LIB) $(OBJECTS_LIB) 
	ar cvq $(OUTPUTFILE) $(OBJECTS_LIB)
	ranlib $(OUTPUTFILE)


clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(OUTPUTFILE)
	
format:
	astyle --style=stroustrup -s4 $(SOURCES) *.h
