#include <unistd.h>
#include <stdlib.h>
#include "input.h"

int process_buffer(int input_text, char **out){
  size_t cap = BUFFER_SIZE;
  size_t total = 0;
  ssize_t n;
  char *buffer = malloc(cap);

  *out = NULL;
  if(buffer == NULL){
    write(STDERR_FILENO,"Allocation failed.\n",19);
    return 1;
  }

  while((n = read(input_text, buffer + total, cap - total - 3)) > 0){
    total += (size_t)n;
    if(total + 3 >= cap){
      char *grown = realloc(buffer, cap * 2);
      if(grown == NULL){
        write(STDERR_FILENO,"Allocation failed.\n",19);
        free(buffer);
        return 1;
      }
      buffer = grown;
      cap *= 2;
    }
  }

  if(n < 0){
    write(STDERR_FILENO,"Error reading file.\n", 20);
    free(buffer);
    return 1;
  }

  if(total == 0){
    write(STDERR_FILENO,"File is empty\n",14);
    free(buffer);
    return 1;
  }

  for(size_t i = 0; i < total; i++){
    unsigned char c = buffer[i];
    if(c < 0x20 && c != '\n' && c != '\t'){
      buffer[i] = ' ';
    }
  }

  buffer[total] = '\n';
  buffer[total+1] = '\n';
  buffer[total+2] = '\0';

  *out = buffer;
  return 0;
}
