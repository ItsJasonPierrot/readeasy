#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/wait.h>
#include "input.h"

enum { SLURP_OK, SLURP_ALLOC, SLURP_READ };

size_t collapse_overstrike(char *buf, size_t n){
  size_t o = 0;
  for(size_t i = 0; i < n; i++){
    if(buf[i] == '\b'){
      if(o > 0 && buf[o-1] != '\n') o--;
    } else {
      buf[o++] = buf[i];
    }
  }
  return o;
}

typedef struct {
  size_t start;
  size_t len;
  int page;
  int drop;
} line_t;

static int line_blank(const char *buf, size_t start, size_t len){
  for(size_t i = 0; i < len; i++)
    if(!isspace((unsigned char)buf[start+i])) return 0;
  return 1;
}

static void line_trim(const char *buf, size_t start, size_t len,
                      size_t *ts, size_t *tl){
  size_t s = start, e = start + len;
  while(s < e && isspace((unsigned char)buf[s])) s++;
  while(e > s && isspace((unsigned char)buf[e-1])) e--;
  *ts = s;
  *tl = e - s;
}

static int trim_equal(const char *buf, line_t a, line_t b){
  size_t as, al, bs, bl;
  line_trim(buf, a.start, a.len, &as, &al);
  line_trim(buf, b.start, b.len, &bs, &bl);
  if(al == 0 || al != bl) return 0;
  return memcmp(buf + as, buf + bs, al) == 0;
}

static int line_all_digits(const char *buf, line_t l){
  size_t ts, tl;
  line_trim(buf, l.start, l.len, &ts, &tl);
  if(tl == 0) return 0;
  for(size_t i = 0; i < tl; i++)
    if(!isdigit((unsigned char)buf[ts+i])) return 0;
  return 1;
}

size_t strip_pdf_furniture(char *buf, size_t n){
  size_t lcap = 256, lcount = 0;
  line_t *L = malloc(lcap * sizeof *L);
  if(L == NULL) return n;

  int page = 0;
  size_t ls = 0;
  for(size_t i = 0; i <= n; i++){
    if(i == n || buf[i] == '\n' || buf[i] == '\f'){
      if(lcount == lcap){
        lcap *= 2;
        line_t *nl = realloc(L, lcap * sizeof *L);
        if(nl == NULL){ free(L); return n; }
        L = nl;
      }
      L[lcount].start = ls;
      L[lcount].len = i - ls;
      L[lcount].page = page;
      L[lcount].drop = 0;
      lcount++;
      if(i < n && buf[i] == '\f') page++;
      ls = i + 1;
      if(i == n) break;
    }
  }
  int npages = page + 1;

  if(npages >= 3){
    long *h0 = malloc((size_t)npages * sizeof(long));
    long *h1 = malloc((size_t)npages * sizeof(long));
    long *t0 = malloc((size_t)npages * sizeof(long));
    long *t1 = malloc((size_t)npages * sizeof(long));
    if(h0 && h1 && t0 && t1){
      for(int p = 0; p < npages; p++){ h0[p] = h1[p] = t0[p] = t1[p] = -1; }
      for(size_t i = 0; i < lcount; i++){
        if(line_blank(buf, L[i].start, L[i].len)) continue;
        int p = L[i].page;
        if(h0[p] < 0) h0[p] = (long)i;
        else if(h1[p] < 0) h1[p] = (long)i;
      }
      for(size_t i = lcount; i-- > 0; ){
        if(line_blank(buf, L[i].start, L[i].len)) continue;
        int p = L[i].page;
        if(t0[p] < 0) t0[p] = (long)i;
        else if(t1[p] < 0) t1[p] = (long)i;
      }
      int ex = npages / 2;
      long *slots[4] = { h0, h1, t0, t1 };
      for(int sidx = 0; sidx < 4; sidx++){
        long *slot = slots[sidx];
        long cand = slot[ex];
        if(cand < 0) continue;
        int count = 0;
        for(int p = 0; p < npages; p++)
          if(slot[p] >= 0 && trim_equal(buf, L[slot[p]], L[cand])) count++;
        if(count * 2 > npages){
          for(int p = 0; p < npages; p++)
            if(slot[p] >= 0 && trim_equal(buf, L[slot[p]], L[cand]))
              L[slot[p]].drop = 1;
        }
      }
      for(int p = 0; p < npages; p++){
        long cands[2] = { h0[p], t0[p] };
        for(int k = 0; k < 2; k++){
          long li = cands[k];
          if(li >= 0 && !L[li].drop && line_all_digits(buf, L[li]))
            L[li].drop = 1;
        }
      }
    }
    free(h0); free(h1); free(t0); free(t1);
  }

  size_t o = 0;
  for(size_t i = 0; i < lcount; i++){
    if(L[i].drop) continue;
    if(L[i].len > 0){
      memmove(buf + o, buf + L[i].start, L[i].len);
      o += L[i].len;
    }
    buf[o++] = '\n';
  }
  free(L);
  return o;
}

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

  total = collapse_overstrike(buffer, total);

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

  total = strip_pdf_furniture(buffer, total);
  total = collapse_overstrike(buffer, total);

  finalize(buffer, total);
  *out = buffer;
  return 0;
}
