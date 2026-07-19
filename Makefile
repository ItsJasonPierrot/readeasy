CC = cc
CFLAGS = -Wall -Wextra -Iinclude

# Use wide-character ncurses (ncursesw) so multi-byte UTF-8 text renders
# correctly instead of as mojibake. Homebrew's ncurses is keg-only on
# macOS (not symlinked onto PATH), so check its opt path too. Falls back
# to plain -lncursesw if no config tool is found.
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

clean:
	rm -f $(OBJ) readeasy
