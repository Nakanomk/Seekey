CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -g
WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
PKGS := gtk4 libevdev gio-unix-2.0 ncursesw json-glib-1.0
LIBS := -ldl

# gtk4-layer-shell must be linked before GTK4 (which pulls in
# libwayland-client), otherwise the static constructor runs too late and
# gtk_layer_init_for_window() will fail.  Detect it via pkg-config and
# add its flags before the main PKGS libs.
# The pc file may be named gtk4-layer-shell-0.pc on some distros.
LAYER_SHELL_PKG := $(shell \
    if $(PKG_CONFIG) --exists gtk4-layer-shell-0 2>/dev/null; then \
        echo gtk4-layer-shell-0; \
    elif $(PKG_CONFIG) --exists gtk4-layer-shell 2>/dev/null; then \
        echo gtk4-layer-shell; \
    fi)
ifdef LAYER_SHELL_PKG
LAYER_SHELL_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(LAYER_SHELL_PKG))
LAYER_SHELL_LIBS  := $(shell $(PKG_CONFIG) --libs $(LAYER_SHELL_PKG))
CFLAGS += $(LAYER_SHELL_CFLAGS)
else
LAYER_SHELL_LIBS :=
endif

PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
DATADIR := $(PREFIX)/share/seekey

TARGET := seekey
SRC := src/main.c src/config.c src/tui.c src/window_state.c \
      src/input.c src/keynames.c src/layer_shell.c
OBJ := $(SRC:.c=.o)

CPPFLAGS += -D_GNU_SOURCE
CFLAGS += $(WARNINGS) $(shell $(PKG_CONFIG) --cflags $(PKGS))
LDLIBS += $(LAYER_SHELL_LIBS) $(shell $(PKG_CONFIG) --libs $(PKGS)) $(LIBS)

# Test target uses the same flags but defines SEEKEY_TEST so tui.c
# skips its ncurses-using code, and links with glib + Unity only.
TEST_BIN := build/test_runner
TEST_DEFS := -DSEEKEY_TEST -DG_DISABLE_ASSERT
TEST_CFLAGS := $(WARNINGS) -O2 -g -Isrc -Itests/vendor/unity $(TEST_DEFS) \
                $(shell $(PKG_CONFIG) --cflags glib-2.0 gobject-2.0 gio-2.0 gtk4 json-glib-1.0)
TEST_SRCS := tests/test_main.c tests/test_config.c tests/test_tui.c \
             tests/test_keynames.c tests/test_window_state.c \
             tests/test_helpers.c
TEST_LDLIBS := $(shell $(PKG_CONFIG) --libs glib-2.0 gobject-2.0 gio-2.0 json-glib-1.0 gtk4) -lm

.PHONY: all clean install uninstall check format

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

%.o: %.c src/seekey.h src/config.h src/tui.h src/window_state.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 seekey.ini.example $(DESTDIR)$(DATADIR)/seekey.ini.example

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(DATADIR)/seekey.ini.example

format:
	@command -v clang-format >/dev/null 2>&1 && \
	    clang-format -i src/*.c src/*.h tests/*.c tests/*.h || \
	    echo "clang-format not installed, skipping"

$(TEST_BIN): $(TEST_SRCS) tests/vendor/unity/unity.c src/config.c src/tui.c src/keynames.c src/window_state.c
	@mkdir -p build
	$(CC) $(TEST_CFLAGS) $(TEST_SRCS) tests/vendor/unity/unity.c \
	    src/config.c src/tui.c src/keynames.c src/window_state.c \
	    -o $@ $(TEST_LDLIBS)

check: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TARGET) $(OBJ) $(TEST_BIN)
