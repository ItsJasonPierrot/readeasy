#include "speech.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

int synth_to_file(const char *text, const char *path, int rate,
                  const char *voice, pid_t *pid){
  *pid = fork();

  if(*pid < 0){
    write(STDERR_FILENO,"fork failed\n",12);
    return 1;
  }

  if(*pid == 0){
    char ratebuf[16];
    snprintf(ratebuf, sizeof ratebuf, "%d", rate);

    char *argv[10];
    int n = 0;
    argv[n++] = "say";
    argv[n++] = "-r";
    argv[n++] = ratebuf;
    if(voice != NULL){
      argv[n++] = "-v";
      argv[n++] = (char *)voice;
    }
    argv[n++] = "-o";
    argv[n++] = (char *)path;
    argv[n++] = (char *)text;
    argv[n]   = NULL;

    execvp("say", argv);
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
