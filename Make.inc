# -*- mode: makefile-gmake -*-
ifneq ($(OS),Windows_NT)
  OS = $(shell uname)
endif

ifneq (,$(findstring MINGW,$(OS)))
override OS := Windows_NT
endif
ifneq (,$(findstring MSYS,$(OS)))
override OS := Windows_NT
endif
ifneq (,$(findstring CYGWIN,$(OS)))
override OS := Windows_NT
endif

CFLAGS-add = -std=gnu99 -Wall

ifeq ($(OS), Linux)
SHLIB_EXT = so
fPIC = -fPIC
endif

ifeq ($(OS), FreeBSD)
SHLIB_EXT = so
fPIC = -fPIC
endif

ifeq ($(OS), OpenBSD)
SHLIB_EXT = so
fPIC = -fPIC
endif

ifeq ($(OS), SunOS)
SHLIB_EXT = so
fPIC = -fPIC
endif

ifeq ($(OS), Darwin)
SHLIB_EXT = dylib
fPIC = -fPIC
endif

ifeq ($(OS), Windows_NT)
SHLIB_EXT = dll
endif

