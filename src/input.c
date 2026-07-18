#include <unistd.h>
#include "input.h"

int process_buffer(int input_text, char *buffer){

  int total = 0;
  ssize_t text;

  while((text = read(input_text,buffer + total, BUFFER_SIZE - total - 3)) > 0){
    total += text;
  }

  if(text<0){
    write(STDERR_FILENO,"Error reading file.\n", 20);
    return 1;
  }

  if(total==0){
    write(STDERR_FILENO,"File is empty\n",14);
    return 1;
  }

  /* Replace stray control bytes (e.g. form feeds from PDF page breaks)
   * with spaces so they don't render as garbage glyphs. Safe for UTF-8:
   * control bytes are all < 0x20, and UTF-8 continuation/lead bytes are
   * always >= 0x80, so this can't corrupt a multi-byte sequence. */
  for(int i = 0; i < total; i++){
    unsigned char c = buffer[i];
    if(c < 0x20 && c != '\n' && c != '\t'){
      buffer[i] = ' ';
    }
  }

  buffer[total] = '\n';
  buffer[total+1] = '\n';
  buffer[total+2] = '\0';

  return 0;
}

