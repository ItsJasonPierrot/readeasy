#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>

#define BUFFER_SIZE 65536

int process_buffer(int input_text, char **out);
int read_pdf(const char *path, char **out);

size_t collapse_overstrike(char *buf, size_t n);
size_t strip_pdf_furniture(char *buf, size_t n);

#endif
