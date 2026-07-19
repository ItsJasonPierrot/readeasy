#ifndef REFLOW_H
#define REFLOW_H

/* Turn raw file text (often hard-wrapped at a fixed column) into a list of
 * sentences suitable for display re-flowing and sentence-by-sentence speech.
 *
 * Hard-wrapped lines are joined back into blocks: a short line (much shorter
 * than the file's widest line, e.g. a heading) ends its block, as does a
 * blank line. Each block is then split into sentences on . ! ? boundaries.
 *
 * Returns a malloc'd array of malloc'd C-strings and sets *nsent. Free with
 * free_sentences(). Returns NULL on allocation failure. */
char **build_sentences(const char *text, int *nsent);

void free_sentences(char **sent, int n);

#endif
