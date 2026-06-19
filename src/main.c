#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(int argc, char *argv[]){
  char buffer[256];
  int input_text = STDIN_FILENO;

  // Argument check
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

  ssize_t text = read(input_text,buffer,sizeof(buffer)-3);

  // Prepare buffer and read opened file from previous block,
  // checking if read returns either -1 or 0, and then adding 
  // newlines so that "say" doesn't stop abruptly on last word

  if(text<0){
    write(STDERR_FILENO,"Error reading file.\n", 20);
    close(input_text);
    return 1;
  }

  if(text==0){
    write(STDERR_FILENO,"File is empty\n",14);
    close(input_text);
    return 1;
  }

  buffer[text] = '\n';
  buffer[text+1] = '\n';
  buffer[text+2] = '\0';

  // Creating new child process
  pid_t pid = fork();

  if(pid < 0){
    write(STDERR_FILENO,"fork failed\n",12);
    close(input_text);
    return 1;
  }

  // Child process performs "say" system call with buffer from read, which perfoms TTS
  if(pid == 0){
    execlp("say","say",buffer,NULL);
    write(STDERR_FILENO,"Child process failed\n",21);
    close(input_text);
    return 1;
  } else {
    wait(NULL);
  }
  
  // File is closed, and program is ended
  close(input_text);

  return 0;
}
