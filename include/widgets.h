#ifndef WIDGETS_H
#define WIDGETS_H

#include <ncurses.h>

int prompt_search(WINDOW *sbar, int cols, char *buf, int cap);
int prompt_text(WINDOW *sbar, int cols, const char *label, char *buf, int cap);
void show_help(int rows, int cols, short color_pair);
int list_picker(int rows, int cols, const char *title,
                char **items, int n, int start, short color_pair);
void draw_settings(WINDOW *w, int sel, int rate, int width, int cols,
                   int theme_idx, int focus, int word_on, const char *voice,
                   int pause_ms, int bionic, int saved);

#endif
