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

