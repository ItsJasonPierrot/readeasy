#ifndef REFLOW_H
#define REFLOW_H

char **build_sentences(const char *text, int *nsent);

void free_sentences(char **sent, int n);

char *wrap_sentence(const char *s, int cols);

#endif
