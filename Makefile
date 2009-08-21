CFLAGS ?= -g -fno-builtin
PROGS = demo

VERSION = "1.0"

all: t.o symtab.c $(PROGS)

OBJS	= dlfcn.o linker.o demo.o demo_main.o symtab.o 
LIBS	= -lpthread -lz
demo: $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

symtab.c: tools/config.example tools/mydeps
	tools/mydeps tools/config.example $@

tools/mydeps: tools/mydeps.c
	$(CC) -o $@ $^


clean:
	rm -f *~ $(PROGS) $(OBJS) t.o symtab.c tools/mydeps tools/ldep/ldep
