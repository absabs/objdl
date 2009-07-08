CFLAGS ?= -Os -march=k8
PROGS = demo

VERSION = "1.0"

all: $(PROGS) t.o

OBJS	= dlfcn.o linker.o demo.o 
LIBS	= -lpthread
demo: $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f *~ $(PROGS) $(OBJS) t.o
