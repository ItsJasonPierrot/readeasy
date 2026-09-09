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

SRC = src/main.c src/input.c src/speech.c src/ui.c src/reflow.c
OBJ = $(SRC:.c=.o)

readeasy: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

install: readeasy
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 readeasy $(DESTDIR)$(BINDIR)/readeasy

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/readeasy

clean:
	rm -f $(OBJ) readeasy

.PHONY: install uninstall clean
