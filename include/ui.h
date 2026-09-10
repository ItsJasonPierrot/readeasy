#ifndef UI_H
#define UI_H

#define RATE_DEFAULT 180
#define RATE_MIN 80
#define RATE_MAX 400

typedef struct {
  int rate;
  const char *voice;
  int color;
  int focus;
} ui_opts;

int run_ui(char *text, const char *name, const ui_opts *opts);

#endif
