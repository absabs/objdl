CFLAGS ?= -g
PROGS = demo

VERSION = "1.0"

all: $(PROGS) t.o sym.map.gz

OBJS	= dlfcn.o linker.o demo.o demo_main.o 
LIBS	= -lpthread -lz
demo: $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
sym.map.gz:demo
	nm -g -fposix $^ |gzip > $@

clean:
	rm -f *~ $(PROGS) $(OBJS) t.o sym.map.gz
