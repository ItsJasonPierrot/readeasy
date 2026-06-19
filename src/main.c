#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(int argc, char *argv[]){
  if(argc < 2){
    write(2,"Usage: ./readeasy <file>\n", 25);
    return 1;
  }

  int file = open(argv[1],O_RDONLY);
  if(file<0){
    write(2,"Could not locate file\n",22);
    return 1;
  }
  
  char buffer[256];
  ssize_t text = read(file,buffer,sizeof(buffer)-3); 
  if(text<0){
    write(2,"Error reading file.\n", 20);
    return 1;
  }
  buffer[text] = '\n';
  buffer[text+1] = '\n';
  buffer[text+2] = '\0';

  pid_t pid = fork();
  if(pid < 0){
    write(2,"fork failed\n",12);
    return 1;
  }

  if(pid == 0){
    execlp("say","say",buffer,NULL);
    write(2,"Child process failed\n",21);
    return 1;
  } else {
    wait(NULL);
  }
  
  close(file);

  return 0;
}
