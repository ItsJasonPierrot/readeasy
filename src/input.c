#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include "input.h"

enum { SLURP_OK, SLURP_ALLOC, SLURP_READ };

static int slurp(int fd, char **buf_out, size_t *total_out){
  size_t cap = BUFFER_SIZE;
  size_t total = 0;
  ssize_t n;
  char *buffer = malloc(cap);

  *buf_out = NULL;
  *total_out = 0;
  if(buffer == NULL) return SLURP_ALLOC;

  while((n = read(fd, buffer + total, cap - total - 3)) > 0){
    total += (size_t)n;
    if(total + 3 >= cap){
      char *grown = realloc(buffer, cap * 2);
      if(grown == NULL){
        free(buffer);
        return SLURP_ALLOC;
      }
      buffer = grown;
      cap *= 2;
    }
  }

  if(n < 0){
    free(buffer);
    return SLURP_READ;
  }

  *buf_out = buffer;
  *total_out = total;
  return SLURP_OK;
}

static void finalize(char *buffer, size_t total){
  for(size_t i = 0; i < total; i++){
    unsigned char c = buffer[i];
    if(c < 0x20 && c != '\n' && c != '\t'){
      buffer[i] = ' ';
    }
  }
  buffer[total] = '\n';
  buffer[total+1] = '\n';
  buffer[total+2] = '\0';
}

int process_buffer(int input_text, char **out){
  char *buffer;
  size_t total;

  *out = NULL;
  switch(slurp(input_text, &buffer, &total)){
    case SLURP_ALLOC:
      write(STDERR_FILENO, "Allocation failed.\n", 19);
      return 1;
    case SLURP_READ:
      write(STDERR_FILENO, "Error reading file.\n", 20);
      return 1;
  }

  if(total == 0){
    write(STDERR_FILENO, "File is empty\n", 14);
    free(buffer);
    return 1;
  }

  finalize(buffer, total);
  *out = buffer;
  return 0;
}

static int command_exists(const char *cmd){
  const char *path = getenv("PATH");
  if(path == NULL) return 0;

  char probe[1024];
  for(const char *p = path; *p; ){
    const char *colon = strchr(p, ':');
    size_t len = colon ? (size_t)(colon - p) : strlen(p);
    if(len > 0 && len + strlen(cmd) + 2 <= sizeof probe){
      memcpy(probe, p, len);
      probe[len] = '/';
      strcpy(probe + len + 1, cmd);
      if(access(probe, X_OK) == 0) return 1;
    }
    if(!colon) break;
    p = colon + 1;
  }
  return 0;
}

int read_pdf(const char *path, char **out){
  *out = NULL;

  if(access(path, R_OK) != 0){
    write(STDERR_FILENO, "File not found.\n", 16);
    return 1;
  }
  if(!command_exists("pdftotext")){
    fputs("readeasy: reading PDFs needs the 'pdftotext' tool.\n"
          "Install it with:  brew install poppler\n", stderr);
    return 1;
  }

  int fds[2];
  if(pipe(fds) != 0){
    write(STDERR_FILENO, "Error reading file.\n", 20);
    return 1;
  }

  pid_t pid = fork();
  if(pid < 0){
    close(fds[0]);
    close(fds[1]);
    write(STDERR_FILENO, "Error reading file.\n", 20);
    return 1;
  }
  if(pid == 0){
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    execlp("pdftotext", "pdftotext", "--", path, "-", (char *)NULL);
    _exit(127);
  }

  close(fds[1]);
  char *buffer;
  size_t total;
  int s = slurp(fds[0], &buffer, &total);
  close(fds[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  if(s == SLURP_ALLOC){
    write(STDERR_FILENO, "Allocation failed.\n", 19);
    return 1;
  }
  if(s == SLURP_READ || !(WIFEXITED(status) && WEXITSTATUS(status) == 0)){
    if(s == SLURP_OK) free(buffer);
    fprintf(stderr, "readeasy: could not read PDF '%s'\n", path);
    return 1;
  }
  if(total == 0){
    free(buffer);
    fprintf(stderr,
            "readeasy: no readable text in '%s'.\n"
            "If it is a scan, it needs OCR before readeasy can read it.\n",
            path);
    return 1;
  }

  finalize(buffer, total);
  *out = buffer;
  return 0;
}
