# -*- mode: makefile -*-

# Default value, can be overriden on the command line
# or with an environment variable.
CC ?= gcc

OS = $(shell uname)

ifneq (,$(findstring MINGW,$(OS)))
override OS := Windows_NT
endif
ifneq (,$(findstring MSYS,$(OS)))
override OS := Windows_NT
endif
ifneq (,$(findstring CYGWIN,$(OS)))
override OS := Windows_NT
endif

ifeq ($(OS), Darwin)
SHLIB_EXT = dylib
else ifeq ($(OS), Windows_NT)
SHLIB_EXT = dll
else
SHLIB_EXT = so
endif

SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:%.c=%.o)
DOBJS = $(SRCS:%.c=%.do)

ifneq ($(MAKECMDGOALS),debug)
XOBJS = $(OBJS)
else
XOBJS = $(DOBJS)
endif

CFLAGS += -DMATHLIB_STANDALONE -fPIC -O3 -std=c99 -I../include

# rmath_tls.c registers a thread-exit destructor so that each thread's
# generator state is released automatically. On Unix that needs pthreads;
# Windows uses FlsAlloc from kernel32 instead, deliberately avoiding a
# runtime dependency on libwinpthread-1.dll. See src/rmath_tls.c.
ifneq ($(OS), Windows_NT)
LIBS += -lpthread
endif

default: release

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DNDEBUG -c $< -o $@
%.do: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -g  -c $< -o $@

release debug: libRmath-julia.$(SHLIB_EXT)

libRmath-julia.$(SHLIB_EXT): $(XOBJS)
	-rm -f $@
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(LIBS)

clean:
	-rm -f *.o *.do *.a *.$(SHLIB_EXT) core* *~ *#
