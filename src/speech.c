#include "speech.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>

static int spawn(char *const argv[], pid_t *pid){
  *pid = fork();
  if(*pid < 0){
    write(STDERR_FILENO, "fork failed\n", 12);
    return 1;
  }
  if(*pid == 0){
    execvp(argv[0], argv);
    write(STDERR_FILENO, "Child process failed\n", 21);
    _exit(127);
  }
  return 0;
}

int synth_to_file(const char *text, const char *path, int rate,
                  const char *voice, pid_t *pid){
  char ratebuf[16];
  snprintf(ratebuf, sizeof ratebuf, "%d", rate);

  char *argv[12];
  int n = 0;
#ifdef __APPLE__
  argv[n++] = "say";
  argv[n++] = "-r";
  argv[n++] = ratebuf;
  if(voice != NULL){ argv[n++] = "-v"; argv[n++] = (char *)voice; }
  argv[n++] = "-o";
  argv[n++] = (char *)path;
  argv[n++] = "--";
  argv[n++] = (char *)text;
#else
  argv[n++] = "espeak-ng";
  argv[n++] = "-s";
  argv[n++] = ratebuf;
  if(voice != NULL){ argv[n++] = "-v"; argv[n++] = (char *)voice; }
  argv[n++] = "-w";
  argv[n++] = (char *)path;
  argv[n++] = "--";
  argv[n++] = (char *)text;
#endif
  argv[n] = NULL;

  return spawn(argv, pid);
}

int play_file(const char *path, pid_t *pid){
#ifdef __APPLE__
  char *argv[] = { "afplay", (char *)path, NULL };
#else
  char *argv[] = { "aplay", "-q", (char *)path, NULL };
#endif
  return spawn(argv, pid);
}

static char *capture_stdout(char *const argv[]){
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
    execvp(argv[0], argv);
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
  return buf;
}

#ifdef __APPLE__
double audio_duration(const char *path){
  char *argv[] = { "afinfo", "--", (char *)path, NULL };
  char *out = capture_stdout(argv);
  if(out == NULL) return -1.0;
  const char *k = strstr(out, "estimated duration:");
  double d = (k != NULL) ? atof(k + 19) : -1.0;
  free(out);
  return d;
}
#else
static unsigned long le32(const unsigned char *p){
  return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
         ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

double audio_duration(const char *path){
  int fd = open(path, O_RDONLY);
  if(fd < 0) return -1.0;
  unsigned char h[1024];
  ssize_t got = read(fd, h, sizeof h);
  close(fd);
  if(got < 12 || memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0)
    return -1.0;

  unsigned long byte_rate = 0, data_size = 0;
  size_t p = 12;
  while(p + 8 <= (size_t)got){
    unsigned long csize = le32(h + p + 4);
    if(memcmp(h + p, "fmt ", 4) == 0){
      if(p + 8 + 16 <= (size_t)got) byte_rate = le32(h + p + 8 + 8);
    } else if(memcmp(h + p, "data", 4) == 0){
      data_size = csize;
      break;
    }
    if(csize == 0) break;
    p += 8 + csize + (csize & 1);
  }
  if(byte_rate == 0 || data_size == 0) return -1.0;
  return (double)data_size / (double)byte_rate;
}
#endif

static int push_voice(char ***arr, int *cnt, int *cap, const char *s, size_t len){
  if(len == 0) return 0;
  if(*cnt == *cap){
    int nc = *cap ? *cap * 2 : 32;
    char **na = realloc(*arr, (size_t)nc * sizeof(char *));
    if(na == NULL) return -1;
    *arr = na;
    *cap = nc;
  }
  char *v = malloc(len + 1);
  if(v == NULL) return -1;
  memcpy(v, s, len);
  v[len] = '\0';
  (*arr)[(*cnt)++] = v;
  return 0;
}

char **list_voices(int *n){
  *n = 0;
#ifdef __APPLE__
  char *argv[] = { "say", "-v", "?", NULL };
#else
  char *argv[] = { "espeak-ng", "--voices", NULL };
#endif
  char *buf = capture_stdout(argv);
  if(buf == NULL) return NULL;

  char **arr = NULL;
  int cnt = 0, cap = 0;
  char *line = buf;
  while(*line){
    char *nl = strchr(line, '\n');
    size_t len = nl ? (size_t)(nl - line) : strlen(line);
#ifdef __APPLE__
    size_t nameend = len;
    for(size_t i = 0; i + 1 < len; i++)
      if(line[i] == ' ' && line[i+1] == ' '){ nameend = i; break; }
    while(nameend > 0 && line[nameend-1] == ' ') nameend--;
    if(push_voice(&arr, &cnt, &cap, line, nameend) < 0) break;
#else
    size_t i = 0;
    while(i < len && isspace((unsigned char)line[i])) i++;
    size_t t0 = i;
    while(i < len && !isspace((unsigned char)line[i])) i++;
    size_t t0e = i;
    while(i < len && isspace((unsigned char)line[i])) i++;
    size_t t1 = i;
    while(i < len && !isspace((unsigned char)line[i])) i++;
    size_t t1e = i;
    int isnum = t0e > t0;
    for(size_t k = t0; k < t0e; k++)
      if(!isdigit((unsigned char)line[k])){ isnum = 0; break; }
    if(isnum && t1e > t1)
      if(push_voice(&arr, &cnt, &cap, line + t1, t1e - t1) < 0) break;
#endif
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
