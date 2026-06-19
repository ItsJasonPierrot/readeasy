#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(int argc, char *argv[]){
  // Argument check
  if(argc < 2){
    write(2,"Usage: ./readeasy <file>\n", 25);
    return 1;
  }
  
  // Open file in Argument 1 (file) and check if contains anything.
  int file = open(argv[1],O_RDONLY);
  if(file<0){
    write(2,"Could not locate file\n",22);
    return 1;
  }
  
  // Prepare buffer and read opened file from previous block, adding newlines so that "say" doesn't stop abruptly on last word
  char buffer[256];
  ssize_t text = read(file,buffer,sizeof(buffer)-3); 
  if(text<0){
    write(2,"Error reading file.\n", 20);
    close(file);
    return 1;
  }
  if(text==0){
    write(2,"File is empty\n",14);
    close(file);
    return 1;
  }
  buffer[text] = '\n';
  buffer[text+1] = '\n';
  buffer[text+2] = '\0';

  // Creating new child process
  pid_t pid = fork();
  if(pid < 0){
    write(2,"fork failed\n",12);
    close(file);
    return 1;
  }

  // Child process performs "say" system call with buffer from read, which perfoms TTS
  if(pid == 0){
    execlp("say","say",buffer,NULL);
    write(2,"Child process failed\n",21);
    close(file);
    return 1;
  } else {
    wait(NULL);
  }
  
  // File is closed, and program is ended
  close(file);
  return 0;
}
