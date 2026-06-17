CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -g
WARNINGS := -Wall -Wextra -Wno-unused-parameter
PKGS := gtk4 libevdev gio-unix-2.0
LIBS := -ldl

TARGET := seekey
SRC := src/main.c src/input.c src/keynames.c src/layer_shell.c
OBJ := $(SRC:.c=.o)

CPPFLAGS += -D_GNU_SOURCE
CFLAGS += $(WARNINGS) $(shell $(PKG_CONFIG) --cflags $(PKGS))
LDLIBS += $(shell $(PKG_CONFIG) --libs $(PKGS)) $(LIBS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

%.o: %.c src/seekey.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
