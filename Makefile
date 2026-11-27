CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Wpedantic
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc $(shell $(PKG_CONFIG) --cflags raylib glu)
LIBS := $(shell $(PKG_CONFIG) --libs raylib glu) -lm
LIB_SOURCES := $(filter-out src/main.c,$(shell find src -name '*.c'))
EXAMPLE_SOURCES := $(shell find examples -name '*.c')
LIB_OBJECTS := $(patsubst %.c,build/%.o,$(LIB_SOURCES))
CLI_OBJECTS := $(patsubst %.c,build/%.o,src/main.c $(EXAMPLE_SOURCES))

.PHONY: all clean test
all: build/manic build/libmanic.a

build/manic: $(CLI_OBJECTS) build/libmanic.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CLI_OBJECTS) build/libmanic.a $(LIBS) $(LDLIBS)

build/libmanic.a: $(LIB_OBJECTS)
	$(AR) rcs $@ $^

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(LIB_OBJECTS:.o=.d) $(CLI_OBJECTS:.o=.d)

test: all
	sh tools/test.sh

clean:
	rm -rf build
