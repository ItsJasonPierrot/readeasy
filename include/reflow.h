#ifndef REFLOW_H
#define REFLOW_H

typedef struct {
  int row;
  int col;
  int cells;
} word_span;

char **build_sentences(const char *text, int *nsent);

void free_sentences(char **sent, int n);

char *wrap_sentence(const char *s, int cols);

int wrap_words(const char *s, int cols, word_span **out);

#endif
