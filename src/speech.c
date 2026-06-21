#include "speech.h"
#include <unistd.h>
#include <sys/wait.h>

int speak(char *text, pid_t *pid){
  *pid = fork();

  if(*pid < 0){
    write(STDERR_FILENO,"fork failed\n",12);
    return 1;
  }

  if(*pid == 0){
    execlp("say","say",text,NULL);
    write(STDERR_FILENO,"Child process failed\n",21);
    return 1;
  }

  return 0;
}
