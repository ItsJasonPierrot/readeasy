CC = cc
CFLAGS = -Wall -Wextra -Iinclude

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

NCURSESW_CONFIG := $(firstword $(wildcard /opt/homebrew/opt/ncurses/bin/ncursesw6-config) \
                                $(wildcard /usr/local/opt/ncurses/bin/ncursesw6-config) \
                                $(shell command -v ncursesw6-config 2>/dev/null))
ifneq ($(NCURSESW_CONFIG),)
  CFLAGS += $(shell $(NCURSESW_CONFIG) --cflags)
  LDLIBS := $(shell $(NCURSESW_CONFIG) --libs)
else
  LDLIBS := -lncursesw
endif

VERSION := $(shell git describe --tags --always --dirty 2>/dev/null)

SRC = src/main.c src/input.c src/speech.c src/ui.c src/reflow.c
OBJ = $(SRC:.c=.o)

readeasy: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

src/main.o: src/main.c
	$(CC) $(CFLAGS) $(if $(VERSION),-DREADEASY_VERSION='"$(VERSION)"') -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: tests/test_reflow.c src/reflow.c
	$(CC) $(CFLAGS) -o tests/test_reflow tests/test_reflow.c src/reflow.c
	./tests/test_reflow

install: readeasy
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 readeasy $(DESTDIR)$(BINDIR)/readeasy

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/readeasy

clean:
	rm -f $(OBJ) readeasy tests/test_reflow

.PHONY: test install uninstall clean
