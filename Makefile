CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
RAYLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags raylib glu)
RAYLIB_LIBS := $(shell $(PKG_CONFIG) --libs raylib glu)
SOURCES := $(shell find src examples -name '*.c')

.PHONY: all clean

all: build/manic

build/manic: $(SOURCES)
	@mkdir -p build
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) src/main.c -o $@ $(LDFLAGS) $(RAYLIB_LIBS) -lm $(LDLIBS)

clean:
	rm -rf build
