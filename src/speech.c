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

char **list_voices(int *n){
  *n = 0;

  int fds[2];
  if(pipe(fds) != 0) return NULL;

  pid_t pid = fork();
  if(pid < 0){
    close(fds[0]);
    close(fds[1]);
    return NULL;
  }
  if(pid == 0){
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    int dn = open("/dev/null", O_WRONLY);
    if(dn >= 0){ dup2(dn, STDERR_FILENO); close(dn); }
    execlp("say", "say", "-v", "?", (char *)NULL);
    _exit(127);
  }

  close(fds[1]);
  char *buf = NULL;
  size_t cap = 0, total = 0;
  char tmp[4096];
  ssize_t r;
  int oom = 0;
  while((r = read(fds[0], tmp, sizeof tmp)) > 0){
    if(total + (size_t)r + 1 > cap){
      size_t nc = cap ? cap : 8192;
      while(nc < total + (size_t)r + 1) nc *= 2;
      char *nb = realloc(buf, nc);
      if(nb == NULL){ oom = 1; break; }
      buf = nb;
      cap = nc;
    }
    memcpy(buf + total, tmp, (size_t)r);
    total += (size_t)r;
  }
  close(fds[0]);
  waitpid(pid, NULL, 0);
  if(buf == NULL || oom){ free(buf); return NULL; }
  buf[total] = '\0';

  char **arr = NULL;
  int cnt = 0, capn = 0;
  char *line = buf;
  while(*line){
    char *nl = strchr(line, '\n');
    size_t len = nl ? (size_t)(nl - line) : strlen(line);

    size_t nameend = len;
    for(size_t i = 0; i + 1 < len; i++){
      if(line[i] == ' ' && line[i+1] == ' '){ nameend = i; break; }
    }
    while(nameend > 0 && line[nameend-1] == ' ') nameend--;

    if(nameend > 0){
      if(cnt == capn){
        int ncap = capn ? capn * 2 : 32;
        char **na = realloc(arr, (size_t)ncap * sizeof(char *));
        if(na == NULL) break;
        arr = na;
        capn = ncap;
      }
      char *v = malloc(nameend + 1);
      if(v == NULL) break;
      memcpy(v, line, nameend);
      v[nameend] = '\0';
      arr[cnt++] = v;
    }

    if(!nl) break;
    line = nl + 1;
  }

  free(buf);
  *n = cnt;
  return arr;
}

void free_voices(char **voices, int n){
  if(voices == NULL) return;
  for(int i = 0; i < n; i++) free(voices[i]);
  free(voices);
}
