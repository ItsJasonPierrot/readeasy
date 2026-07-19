#include "reflow.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Grow *buf to at least `need` bytes. Returns 0 on success, -1 on failure. */
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

/* Append a trimmed copy of s[0..len) to the sentence array. Empty after
 * trimming is ignored. Returns 0 on success, -1 on failure. */
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

/* Split a single-spaced block into sentences. A sentence ends at . ! ? that
 * is followed by whitespace or end-of-block (so decimals and "e.g." don't
 * split), including any closing quote/bracket that trails the punctuation. */
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
        k = m;              /* not a real boundary; keep scanning */
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

char **build_sentences(const char *text, int *nsent){
  char **arr = NULL;
  int n = 0, cap = 0;

  /* Widest line in the file ~= the wrap column. Lines much shorter than this
   * are treated as deliberate breaks (headings, paragraph ends). */
  int maxw = 0, run = 0;
  for(const char *p = text; ; p++){
    if(*p == '\n' || *p == '\0'){
      if(run > maxw) maxw = run;
      run = 0;
      if(*p == '\0') break;
    } else {
      run++;
    }
  }
  int threshold = maxw * 3 / 5;
  if(threshold < 1) threshold = 1;

  char *para = NULL;
  size_t plen = 0, pcap = 0;
  const char *linestart = text;
  int failed = 0;

  for(const char *p = text; ; p++){
    if(*p != '\n' && *p != '\0') continue;

    size_t ll = (size_t)(p - linestart);
    while(ll > 0 && linestart[ll-1] == '\r') ll--;

    int blank = 1;
    for(size_t q = 0; q < ll; q++)
      if(!isspace((unsigned char)linestart[q])){ blank = 0; break; }

    if(blank){
      if(plen > 0){
        if(split_block(&arr, &n, &cap, para, plen) < 0){ failed = 1; break; }
        plen = 0;
      }
    } else {
      if(plen > 0){
        if(ensure(&para, &pcap, plen + 1) < 0){ failed = 1; break; }
        para[plen++] = ' ';
      }
      if(ensure(&para, &pcap, plen + ll) < 0){ failed = 1; break; }
      memcpy(para + plen, linestart, ll);
      plen += ll;

      if((int)ll < threshold){        /* short line ends the block */
        if(split_block(&arr, &n, &cap, para, plen) < 0){ failed = 1; break; }
        plen = 0;
      }
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
