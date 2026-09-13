#include "speech.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
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

    char *argv[12];
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
    argv[n++] = "--";
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

double audio_duration(const char *path){
  int fds[2];
  if(pipe(fds) != 0) return -1.0;

  pid_t pid = fork();
  if(pid < 0){
    close(fds[0]);
    close(fds[1]);
    return -1.0;
  }
  if(pid == 0){
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    int dn = open("/dev/null", O_WRONLY);
    if(dn >= 0){ dup2(dn, STDERR_FILENO); close(dn); }
    execlp("afinfo", "afinfo", "--", path, (char *)NULL);
    _exit(127);
  }

  close(fds[1]);
  char buf[4096];
  size_t total = 0;
  ssize_t n;
  while(total < sizeof buf - 1 &&
        (n = read(fds[0], buf + total, sizeof buf - 1 - total)) > 0)
    total += (size_t)n;
  close(fds[0]);
  waitpid(pid, NULL, 0);
  buf[total] = '\0';

  const char *k = strstr(buf, "estimated duration:");
  if(k == NULL) return -1.0;
  return atof(k + 19);
}
