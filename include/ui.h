#ifndef UI_H
#define UI_H

typedef struct {
  int rate;          /* starting words per minute */
  const char *voice; /* TTS voice, or NULL for the default */
  int color;         /* 1 = apply the color theme, 0 = plain */
} ui_opts;

int run_ui(char *text, const char *name, const ui_opts *opts);

#endif
