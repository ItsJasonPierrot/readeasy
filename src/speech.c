#include "speech.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int synth_to_file(const char *text, const char *path, int rate, pid_t *pid){
  *pid = fork();

  if(*pid < 0){
    write(STDERR_FILENO,"fork failed\n",12);
    return 1;
  }

  if(*pid == 0){
    char ratebuf[16];
    snprintf(ratebuf, sizeof ratebuf, "%d", rate);
    execlp("say","say","-r",ratebuf,"-o",path,text,(char*)NULL);
    write(STDERR_FILENO,"Child process failed\n",21);
    _exit(127);
  }

  return 0;
}

int play_file(const char *path, pid_t *pid){
  *pid = fork();

  if(*pid < 0){
    write(STDERR_FILENO,"fork failed\n",12);
    return 1;
  }

  if(*pid == 0){
    execlp("afplay","afplay",path,(char*)NULL);
    write(STDERR_FILENO,"Child process failed\n",21);
    _exit(127);
  }

  return 0;
}
