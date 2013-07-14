

CFLAGS=-std=c99 -Wall -static  -L. 
CLIBS=-losmpbf -lpthread  -lz  -lprotobuf-c 
CCFLAGS=-I. 

all: libosmaccess.a

test: testosmaccess


libosmpbf.a: fileformat.pb-c.o osmformat.pb-c.o
	$(AR) -cr $@ $^

libosmaccess.a: libosmaccess.o
	ar rcs $@ $^
libosmaccess.o: libosmaccess.c libosmpbf.a
	$(CC) -c $(CFLAGS) $< $(CLIBS) -static -o $@

testosmaccess: testosmaccess.c libosmaccess.a
	$(CC) $(CFLAGS) $< -l osmaccess $(CLIBS)  -o $@


clean:
	rm testosmaccess libosmaccess.a *.o