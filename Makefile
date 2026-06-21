readeasy: src/main.c src/input.c src/speech.c src/ui.c
	cc -Iinclude src/main.c src/input.c src/speech.c src/ui.c -o readeasy -lncurses

clean:
	rm -f readeasy
