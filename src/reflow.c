#include "reflow.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int ensure(char **buf, size_t *cap, size_t need){
  if(*cap >= need) return 0;
  size_t nc = *cap ? *cap : 128;
  while(nc < need) nc *= 2;
  char *nb = realloc(*buf, nc);
  if(nb == NULL) return -1;
  *buf = nb;
  *cap = nc;
  return 0;
}

static int push_sentence(char ***arr, int *n, int *cap, const char *s, size_t len){
  while(len > 0 && isspace((unsigned char)s[0]))       { s++; len--; }
  while(len > 0 && isspace((unsigned char)s[len-1]))    { len--; }
  if(len == 0) return 0;

  if(*n == *cap){
    int nc = *cap ? *cap * 2 : 32;
    char **na = realloc(*arr, (size_t)nc * sizeof(char *));
    if(na == NULL) return -1;
    *arr = na;
    *cap = nc;
  }
  char *d = malloc(len + 1);
  if(d == NULL) return -1;
  memcpy(d, s, len);
  d[len] = '\0';
  (*arr)[(*n)++] = d;
  return 0;
}

static int split_block(char ***arr, int *n, int *cap, const char *b, size_t blen){
  size_t i = 0;
  while(i < blen){
    size_t k = i, end = blen;
    int found = 0;
    while(k < blen){
      if(b[k] == '.' || b[k] == '!' || b[k] == '?'){
        size_t m = k + 1;
        while(m < blen && (b[m] == '"' || b[m] == '\'' ||
                           b[m] == ')' || b[m] == ']')) m++;
        if(m >= blen || isspace((unsigned char)b[m])){
          end = m;
          found = 1;
          break;
        }
        k = m;
      } else {
        k++;
      }
    }
    if(!found) end = blen;
    if(push_sentence(arr, n, cap, b + i, end - i) < 0) return -1;
    i = end;
    while(i < blen && isspace((unsigned char)b[i])) i++;
  }
  return 0;
}

static int line_all_caps(const char *s, size_t len){
  int has_upper = 0;
  for(size_t i = 0; i < len; i++){
    unsigned char c = (unsigned char)s[i];
    if(c >= 'a' && c <= 'z') return 0;
    if(c >= 'A' && c <= 'Z') has_upper = 1;
  }
  return has_upper;
}

char **build_sentences(const char *text, int *nsent){
  char **arr = NULL;
  int n = 0, cap = 0;

  char *para = NULL;
  size_t plen = 0, pcap = 0;
  const char *linestart = text;
  int failed = 0;

  for(const char *p = text; ; p++){
    if(*p != '\n' && *p != '\0') continue;

    size_t ll = (size_t)(p - linestart);
    while(ll > 0 && linestart[ll-1] == '\r') ll--;

    size_t ts = 0;
    while(ts < ll && isspace((unsigned char)linestart[ts])) ts++;
    size_t te = ll;
    while(te > ts && isspace((unsigned char)linestart[te-1])) te--;
    const char *line = linestart + ts;
    size_t tlen = te - ts;

    int blank = (tlen == 0);
    int heading = !blank && line_all_caps(line, tlen);

    if(blank || heading){
      if(plen > 0){
        if(split_block(&arr, &n, &cap, para, plen) < 0){ failed = 1; break; }
        plen = 0;
      }
      if(heading && split_block(&arr, &n, &cap, line, tlen) < 0){
        failed = 1;
        break;
      }
    } else {
      if(plen > 0){
        if(para[plen-1] == '-' && plen >= 2 &&
           isalpha((unsigned char)para[plen-2]) &&
           isalpha((unsigned char)line[0])){
          plen--;
        } else {
          if(ensure(&para, &pcap, plen + 1) < 0){ failed = 1; break; }
          para[plen++] = ' ';
        }
      }
      if(ensure(&para, &pcap, plen + tlen) < 0){ failed = 1; break; }
      memcpy(para + plen, line, tlen);
      plen += tlen;
    }

    if(*p == '\0'){
      if(!failed && plen > 0)
        if(split_block(&arr, &n, &cap, para, plen) < 0) failed = 1;
      break;
    }
    linestart = p + 1;
  }

  free(para);

  if(failed){
    free_sentences(arr, n);
    *nsent = 0;
    return NULL;
  }
  *nsent = n;
  return arr;
}

void free_sentences(char **sent, int n){
  if(sent == NULL) return;
  for(int i = 0; i < n; i++) free(sent[i]);
  free(sent);
}

char *wrap_sentence(const char *s, int cols){
  if(cols < 1) cols = 1;

  char *out = NULL;
  size_t cap = 0, len = 0;
  int col = 0;
  const char *p = s;

  while(*p){
    while(*p == ' ') p++;
    if(!*p) break;

    const char *w = p;
    int wlen = 0;
    while(*p && *p != ' '){
      p++;
      while(((unsigned char)*p & 0xC0) == 0x80) p++;
      wlen++;
    }
    size_t wbytes = (size_t)(p - w);

    if(col > 0){
      char sep;
      if(col + 1 + wlen <= cols){ sep = ' '; col += 1; }
      else                      { sep = '\n'; col = 0; }
      if(ensure(&out, &cap, len + 1) < 0){ free(out); return NULL; }
      out[len++] = sep;
    }

    if(wlen <= cols){
      if(ensure(&out, &cap, len + wbytes) < 0){ free(out); return NULL; }
      memcpy(out + len, w, wbytes);
      len += wbytes;
      col += wlen;
    } else {
      const char *cp = w, *end = w + wbytes;
      while(cp < end){
        const char *st = cp;
        cp++;
        while(cp < end && ((unsigned char)*cp & 0xC0) == 0x80) cp++;
        if(col >= cols){
          if(ensure(&out, &cap, len + 1) < 0){ free(out); return NULL; }
          out[len++] = '\n';
          col = 0;
        }
        size_t cb = (size_t)(cp - st);
        if(ensure(&out, &cap, len + cb) < 0){ free(out); return NULL; }
        memcpy(out + len, st, cb);
        len += cb;
        col += 1;
      }
    }
  }

  if(ensure(&out, &cap, len + 1) < 0){ free(out); return NULL; }
  out[len] = '\0';
  return out;
}

int wrap_words(const char *s, int cols, word_span **out){
  *out = NULL;

  char *w = wrap_sentence(s, cols);
  if(w == NULL) return 0;

  word_span *arr = NULL;
  int cap = 0, n = 0;
  int row = 0, col = 0;
  int in = 0, tstart = 0, tcells = 0;

  for(const char *p = w; ; ){
    unsigned char c = (unsigned char)*p;
    if(c == '\0' || c == '\n' || c == ' '){
      if(in){
        if(n == cap){
          int nc = cap ? cap * 2 : 16;
          word_span *na = realloc(arr, (size_t)nc * sizeof(word_span));
          if(na == NULL){ free(arr); free(w); return 0; }
          arr = na;
          cap = nc;
        }
        arr[n].row = row;
        arr[n].col = tstart;
        arr[n].cells = tcells;
        n++;
        in = 0;
      }
      if(c == '\0') break;
      if(c == '\n'){ row++; col = 0; }
      else col++;
      p++;
    } else {
      if(!in){ in = 1; tstart = col; tcells = 0; }
      p++;
      while(((unsigned char)*p & 0xC0) == 0x80) p++;
      col++;
      tcells++;
    }
  }

  free(w);
  *out = arr;
  return n;
}
