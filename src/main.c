#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include "input.h"
#include "ui.h"

int main(int argc, char *argv[]){
  char *buffer;
  int input_text = STDIN_FILENO;

  if(argc > 2){
    write(STDERR_FILENO, "Usage: readeasy <filename>\n", 27);
    return 1;
  }

  if(argc == 1 && isatty(STDIN_FILENO)){
    write(STDERR_FILENO, "No input provided.\n",19);
    return 1;
  }

  if(argc == 2){
    input_text = open(argv[1], O_RDONLY);
    if(input_text < 0){
      write(STDERR_FILENO, "File not found.\n", 16);
      return 1;
    }
  }

  if(process_buffer(input_text, &buffer) != 0){
    if(argc == 2) close(input_text);
    return 1;
  }

  if(argc == 2) close(input_text);

  if(!isatty(STDIN_FILENO)){
    if(freopen("/dev/tty", "r", stdin) == NULL){
      write(STDERR_FILENO, "No terminal available for controls.\n", 36);
      free(buffer);
      return 1;
    }
  }

  if(run_ui(buffer) != 0){
    free(buffer);
    return 1;
  }

  free(buffer);

  return 0;
}
