# Prerequisites
#STATIC_GLFW_LIB = /home/adrien/Documents/perso/glfw-3.1.1/src/libglfw3.a
GLFW_INCLUDE = $(shell pkg-config --cflags glfw3)
GLFW_LIBS = $(shell pkg-config --static --libs glfw3)
ASSIMP_INCLUDE = $(shell pkg-config --cflags assimp)
ASSIMP_LIBS = $(shell pkg-config --libs assimp)
FREETYPE_INCLUDE = $(shell pkg-config --cflags freetype2)
FREETYPE_LIBS = $(shell pkg-config --libs freetype2)
PNG_INCLUDE = $(shell pkg-config --cflags libpng)
PNG_LIBS = $(shell pkg-config --libs libpng)
ZLIB_INCLUDE = $(shell pkg-config --cflags zlib)
ZLIB_LIBS = $(shell pkg-config --libs zlib)
IMGUI_INCLUDE = -Iext/imgui

# Config
COMMON_FLAGS = -Isrc -Iext -Iext/freetype $(IMGUI_INCLUDE) $(GLFW_INCLUDE) $(ASSIMP_INCLUDE) $(FREETYPE_INCLUDE) $(PNG_INCLUDE) $(ZLIB_INCLUDE) -DPNG_SKIP_SETJMP_CHECK -std=c++11 $(OPTFLAGS)
DEBUG_FLAGS = -g -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -D_DEBUG $(COMMON_FLAGS)
RELEASE_FLAGS = -O2 -D_NDEBUG $(COMMON_FLAGS)

CC = g++
PREFIX ?= /usr/local
LIBS = $(GLFW_LIBS) -lGL $(ASSIMP_LIBS) $(FREETYPE_LIBS) $(PNG_LIBS) $(ZLIB_LIBS) -lm $(OPTLIBS)

# Config LIBRADAR
LIB_TARGET = bin/libradar.a
LIB_CFLAGS = $(DEBUG_FLAGS)

LIB_MODULES = common render_internal testdir
LIB_SOURCES = $(wildcard src/**/*.cpp src/*.cpp)
LIB_OBJECTS = $(patsubst src/%.cpp,build/%.o,$(LIB_SOURCES))

LIB_EXT_MODULES = imgui
LIB_EXT_GLEW = build/glew.o
LIB_EXT_JSON = build/cJSON.o
LIB_EXT_IMGUI_SRC = $(wildcard ext/imgui/*.cpp)
LIB_EXT_IMGUI = $(patsubst ext/%.cpp,build/%.o,$(LIB_EXT_IMGUI_SRC))
LIB_EXT_OBJECTS = $(LIB_EXT_GLEW) $(LIB_EXT_JSON) $(LIB_EXT_IMGUI)


# Config Sample
TARGET = bin/radar
CFLAGS = $(DEBUG_FLAGS)

TEST_SRC = $(wildcard tests/*_tests.c)
TESTS = $(patsubst %.c,%,$(TEST_SRC))


.PHONY: depend, tests, release, clean, install, check, external

all: lib

release: LIB_CFLAGS = $(RELEASE_FLAGS)
release: all

depend:
	makedepend -- $(LIB_CFLAGS) -- $(LIB_SOURCES)

prereq:
#	@test -s $(STATIC_GLFW_LIB) || { echo "GLFW3 library does not exist($(STATIC_GLFW_LIB)). Exiting..."; false; }

#------------------------------------------------------------------
external: $(LIB_EXT_MODULES) $(LIB_EXT_OBJECTS)

$(LIB_EXT_MODULES):
	@mkdir -p build/$@

$(LIB_EXT_JSON): ext/json/cJSON.c
	@echo "CC		$@"
	@g++ -O2 -D_NDEBUG -c $< -o $@

$(LIB_EXT_GLEW): ext/GL/glew.c
	@echo "CC		$@"
	@g++ -O2 -D_NDEBUG -c $< -o $@

$(LIB_EXT_IMGUI): build/%.o: ext/%.cpp
	@echo "CC		$@"
	@$(CC) $(LIB_CFLAGS) -c $< -o $@

#------------------------------------------------------------------
lib: $(LIB_MODULES) external $(LIB_OBJECTS)
	@echo "AR		$@"
	@ar -rcs $(LIB_TARGET) $(LIB_OBJECTS) $(LIB_EXT_OBJECTS)

$(LIB_MODULES):
	@mkdir -p build/$@
	@mkdir -p bin

$(LIB_OBJECTS): build/%.o: src/%.cpp
	@echo "CC		$@"
	@ $(CC) $(LIB_CFLAGS) -c $< -o $@

#------------------------------------------------------------------
sample: lib
	@echo "CC		$(TARGET)"
	@$(CC) $(CFLAGS) main.cpp -Lbin/ -lradar $(LIBS) -o $(TARGET)

clean:
	rm $(LIB_OBJECTS)

cleanall:
	rm -rf build

install: all
	@echo "Nothing set to be installed!"
