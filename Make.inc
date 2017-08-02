# -*- mode: makefile-gmake -*-
ifeq ($(OS),Windows_NT)
  RM = cmd /C del /Q /F
else
  RM = rm -f
  OS = $(shell uname)
endif


FC = gfortran
FFLAGS-add += -O3

USEGCC = 1
USECLANG = 0

CFLAGS-add += -Wall -O3

ifeq ($(USECLANG),1)
USEGCC = 0
CC = clang
endif

ifeq ($(USEGCC),1)
CC = gcc
endif

ifeq ($(OS), Linux)
SHLIB_EXT = so
CFLAGS-add+=-fPIC
FFLAGS-add+=-fPIC
endif

ifeq ($(OS), FreeBSD)
SHLIB_EXT = so
CFLAGS-add+=-fPIC
FFLAGS-add+=-fPIC
endif

ifeq ($(OS), OpenBSD)
SHLIB_EXT = so
CFLAGS-add+=-fPIC
FFLAGS-add+=-fPIC
endif

ifeq ($(OS), Darwin)
SHLIB_EXT = dylib
CFLAGS-add+=-fPIC
FFLAGS-add+=-fPIC
endif

ifeq ($(OS), Windows_NT)
SHLIB_EXT = dll
endif

# SunOS with gcc
ifeq ($(OS), SunOS)
SHLIB_EXT = so
CFLAGS-add+=-fPIC
FFLAGS-add+=-fPIC
endif
