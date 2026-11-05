# Compiler and installation settings.
CC		 =	cc
CPPFLAGS	+=	-Isrc/shared \
			$(shell pkg-config --cflags xft xpm libpng) \
			-D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -MMD -MP
CFLAGS		?=	-O2 -g
CFLAGS		+=	-std=c89 -Wall -Wextra -Wpedantic -pedantic-errors \
			-Wmissing-prototypes -Wstrict-prototypes
LDLIBS		+=	-lX11 $(shell pkg-config --libs xft)
IMAGE_LIBS	 =	$(shell pkg-config --libs xpm libpng)
PREFIX		?=	/usr/local

# Application builds.
SHARED_OBJ	 =	$(patsubst src/%.c,build/%.o,$(wildcard src/shared/*.c))
WM_OBJ		 =	$(patsubst src/%.c,build/%.o,$(wildcard src/wm/*.c)) \
			$(SHARED_OBJ)
RUN_OBJ		 =	$(patsubst src/%.c,build/%.o,$(wildcard src/run/*.c)) \
			$(SHARED_OBJ)

.PHONY: all clean install
all: owm owm-run

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Compile the shipped shortcuts into the WM, with no runtime config dependency.
build/wm/default-bindings.h: config scripts/default-bindings.awk
	@mkdir -p $(@D)
	awk -f scripts/default-bindings.awk config > $@.tmp
	mv $@.tmp $@

build/wm/config-entries.o: build/wm/default-bindings.h
build/wm/config-entries.o: CPPFLAGS += -Ibuild/wm

owm: $(WM_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(IMAGE_LIBS)

owm-run: $(RUN_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Installation and cleanup.
install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 owm owm-run $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f owm owm-run
	rm -rf build

# Formatting.
SOURCES		 =	$(wildcard src/*/*.c src/*/*.h tests/*.c \
			tests/integration/*.c tests/integration/*.h)
CLANG_FORMAT	?=	clang-format
.PHONY: format format-check
format:
	./scripts/format --clang-format "$(CLANG_FORMAT)" $(SOURCES)
format-check:
	CLANG_FORMAT="$(CLANG_FORMAT)" python3 tests/format.py
	./scripts/format --check --clang-format "$(CLANG_FORMAT)" $(SOURCES)

# Tests.
UNIT_TESTS	 =	core config icon-positions ui-palette session-file window-positions \
			tray-widgets tray-wireguard wait
TEST_PROGRAMS	 =	$(addprefix build/tests/,\
			$(UNIT_TESTS) integration sdl-fullscreen)

# SDL2 is optional and used only for the game compatibility regression.
SDL_TEST	 =	$(shell pkg-config --exists sdl2 && \
			echo build/tests/sdl-fullscreen)

.PHONY: test
test: all $(addprefix build/tests/,$(UNIT_TESTS)) \
	build/tests/integration $(SDL_TEST)
	@set -e; for test in $(UNIT_TESTS); do ./build/tests/$$test; done
	./tests/run.sh

# Every test uses the same compile and link rules. Dependencies follow below.
build/test-objects/%.o: tests/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(TEST_PROGRAMS): build/tests/%: build/test-objects/%.o
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(TEST_LIBS)

build/tests/core: $(SHARED_OBJ)
build/tests/core: TEST_LIBS = $(LDLIBS)

build/tests/config: build/wm/config.o build/wm/config-entries.o \
	build/wm/action-parse.o build/shared/builtin-icons.o \
	build/shared/memory.o build/shared/paths.o \
	build/shared/settings.o build/shared/string-util.o \
	build/shared/ui-colors.o build/shared/ui-config.o
build/tests/config: TEST_LIBS = -lX11

build/tests/icon-positions: build/wm/icon-positions.o \
	build/shared/file-replace.o build/shared/paths.o \
	build/shared/settings.o build/shared/memory.o build/shared/string-util.o

build/tests/ui-palette: build/shared/ui-palette.o build/shared/ui-colors.o

build/tests/session-file: build/wm/session-file.o build/shared/memory.o \
	build/shared/settings.o build/shared/string-util.o

build/tests/window-positions: build/wm/window-positions.o \
	build/shared/memory.o build/shared/settings.o build/shared/string-util.o

build/tests/tray-widgets: build/shared/builtin-icons.o build/shared/memory.o \
	build/shared/paths.o build/shared/settings.o \
	build/shared/string-util.o build/wm/tray-battery.o \
	build/wm/tray-clock.o build/wm/tray-wireguard.o
build/tests/tray-widgets: TEST_LIBS = $(LDLIBS)

build/tests/tray-wireguard: build/shared/string-util.o

INTEGRATION_SRC	 =	$(wildcard tests/integration/*.c)
INTEGRATION_OBJ	 =	$(patsubst tests/%.c,build/test-objects/%.o,\
			$(INTEGRATION_SRC))
build/tests/integration: $(INTEGRATION_OBJ) build/shared/ui-config.o \
	build/shared/font.o build/shared/memory.o build/shared/paths.o \
	build/shared/settings.o build/shared/string-util.o
build/tests/integration: TEST_LIBS = $(LDLIBS) \
	$(shell pkg-config --libs libpng) -ldl

build/test-objects/sdl-fullscreen.o: CPPFLAGS += \
	$(shell pkg-config --cflags sdl2)
build/tests/sdl-fullscreen: TEST_LIBS = $(shell pkg-config --libs sdl2) -lX11

# Generated header dependencies.
-include $(wildcard build/shared/*.d build/wm/*.d build/run/*.d \
	build/test-objects/*.d build/test-objects/integration/*.d)
