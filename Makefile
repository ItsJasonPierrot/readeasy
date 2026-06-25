CC = cc
CFLAGS = -Wall -Wextra -Iinclude
LDLIBS = -lncurses

SRC = src/main.c src/input.c src/speech.c src/ui.c
OBJ = $(SRC:.c=.o)

readeasy: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) readeasy
