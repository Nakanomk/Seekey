CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -g
WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
PKGS := gtk4 libevdev gio-unix-2.0 ncursesw
LIBS := -ldl

# gtk4-layer-shell must be linked before GTK4 (which pulls in
# libwayland-client), otherwise the static constructor runs too late and
# gtk_layer_init_for_window() will fail.  Detect it via pkg-config and
# add its flags before the main PKGS libs.
HAVE_LAYER_SHELL := $(shell $(PKG_CONFIG) --exists gtk4-layer-shell 2>/dev/null && echo 1 || echo 0)
ifeq ($(HAVE_LAYER_SHELL),1)
LAYER_SHELL_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk4-layer-shell)
LAYER_SHELL_LIBS  := $(shell $(PKG_CONFIG) --libs gtk4-layer-shell)
CFLAGS += $(LAYER_SHELL_CFLAGS)
else
LAYER_SHELL_LIBS :=
endif

TARGET := seekey
SRC := src/main.c src/input.c src/keynames.c src/layer_shell.c
OBJ := $(SRC:.c=.o)

CPPFLAGS += -D_GNU_SOURCE
CFLAGS += $(WARNINGS) $(shell $(PKG_CONFIG) --cflags $(PKGS))
LDLIBS += $(LAYER_SHELL_LIBS) $(shell $(PKG_CONFIG) --libs $(PKGS)) $(LIBS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

%.o: %.c src/seekey.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
