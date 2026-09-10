#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reflow.h"

static int pass = 0, fail = 0;

static void ok(const char *name, int cond){
  printf("%s  %s\n", cond ? "pass" : "FAIL", name);
  if(cond) pass++; else fail++;
}

static int dispw(const char *s, int n){
  int w = 0;
  for(int i = 0; i < n; ){
    unsigned char c = s[i];
    int l = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
    i += l; w++;
  }
  return w;
}

static int lines_fit(const char *w, int cols){
  const char *p = w;
  while(*p){
    const char *nl = strchr(p, '\n');
    int n = nl ? (int)(nl - p) : (int)strlen(p);
    if(dispw(p, n) > cols) return 0;
    if(!nl) break;
    p = nl + 1;
  }
  return 1;
}

static int words_equal(const char *a, const char *b){
  char A[8192], B[8192];
  int ai = 0, bi = 0;
  for(const char *p = a; *p; p++){
    if(*p == ' ' || *p == '\n'){ if(ai && A[ai-1] != ' ') A[ai++] = ' '; }
    else A[ai++] = *p;
  }
  for(const char *p = b; *p; p++){
    if(*p == ' ' || *p == '\n'){ if(bi && B[bi-1] != ' ') B[bi++] = ' '; }
    else B[bi++] = *p;
  }
  while(ai && A[ai-1] == ' ') ai--;
  while(bi && B[bi-1] == ' ') bi--;
  A[ai] = '\0'; B[bi] = '\0';
  return strcmp(A, B) == 0;
}

static void test_wrap(void){
  const char *s = "the quick brown fox jumps over the lazy dog again today here";
  for(int cols = 10; cols <= 40; cols += 6){
    char *w = wrap_sentence(s, cols);
    char nm[80];
    snprintf(nm, sizeof nm, "wrap: no line over %d cols", cols);
    ok(nm, w && lines_fit(w, cols));
    snprintf(nm, sizeof nm, "wrap: no word split at %d", cols);
    ok(nm, w && words_equal(w, s));
    free(w);
  }

  char *lw = wrap_sentence("short SUPERCALIFRAGILISTICEXPIALIDOCIOUS end", 12);
  ok("wrap: overlong word hard-broken within width", lw && lines_fit(lw, 12));
  free(lw);

  char *e = wrap_sentence("   ", 20);
  ok("wrap: whitespace-only becomes empty", e && e[0] == '\0');
  free(e);

  char *u = wrap_sentence("cafe resume naive Noel widely spaced words here now go", 20);
  ok("wrap: utf-8-ish text fits width", u && lines_fit(u, 20));
  free(u);
}

static void test_split(void){
  int n;
  char **s;

  s = build_sentences("One. Two. Three.", &n);
  ok("split: three sentences", n == 3);
  ok("split: first is 'One.'", n > 0 && !strcmp(s[0], "One."));
  ok("split: last is 'Three.'", n > 2 && !strcmp(s[2], "Three."));
  free_sentences(s, n);

  s = build_sentences("The value 3.5 is used. Next one here.", &n);
  ok("split: decimal is not a boundary",
     n == 2 && !strcmp(s[0], "The value 3.5 is used."));
  free_sentences(s, n);

  s = build_sentences("Read this! Are you sure? Yes.", &n);
  ok("split: ! and ? are boundaries", n == 3);
  free_sentences(s, n);
}

static void test_reflow(void){
  int n;
  char **s;

  const char *hw =
    "aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa\n"
    "bbbb bbbb here.\n";
  s = build_sentences(hw, &n);
  ok("reflow: hard-wrapped lines rejoined into one sentence", n == 1);
  ok("reflow: rejoined text keeps both lines",
     n == 1 && strstr(s[0], "aaaa") && strstr(s[0], "bbbb bbbb here."));
  free_sentences(s, n);

  const char *hd =
    "Title\n"
    "This is the body sentence that runs on for a good while here now today.\n";
  s = build_sentences(hd, &n);
  ok("reflow: short heading kept separate", n == 2 && !strcmp(s[0], "Title"));
  free_sentences(s, n);

  s = build_sentences("   \n\n  ", &n);
  ok("reflow: whitespace-only yields no sentences", n == 0);
  free_sentences(s, n);
}

int main(void){
  test_wrap();
  test_split();
  test_reflow();
  printf("\n%d passed, %d failed\n", pass, fail);
  return fail != 0;
}
