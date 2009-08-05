CFLAGS ?= -g
PROGS = demo

VERSION = "1.0"

all: $(PROGS) t.o sym.map

OBJS	= dlfcn.o linker.o demo.o 
LIBS	= -lpthread
demo: $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
sym.map:demo
	nm -g $^ > $@

clean:
	rm -f *~ $(PROGS) $(OBJS) t.o sym.map
