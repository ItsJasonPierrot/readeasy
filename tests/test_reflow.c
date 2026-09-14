#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reflow.h"
#include "input.h"

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
    "\n"
    "This is the body sentence that runs on for a good while here now today.\n";
  s = build_sentences(hd, &n);
  ok("reflow: blank-separated heading kept separate",
     n == 2 && !strcmp(s[0], "Title"));
  free_sentences(s, n);

  const char *caps =
    "NAME\n"
    "ls lists the files in a directory.\n";
  s = build_sentences(caps, &n);
  ok("reflow: ALL-CAPS heading split off even without a blank line",
     n == 2 && !strcmp(s[0], "NAME"));
  free_sentences(s, n);

  const char *split =
    "This one sentence has been hard-\n"
    "wrapped onto a short second line.\n";
  s = build_sentences(split, &n);
  ok("reflow: a wrapped sentence is not cut in half", n == 1);
  ok("reflow: hyphen at a line break is rejoined",
     n == 1 && strstr(s[0], "hardwrapped") != NULL);
  free_sentences(s, n);

  const char *two =
    "The first sentence wraps over\n"
    "several lines here. The second\n"
    "sentence also wraps over lines.\n";
  s = build_sentences(two, &n);
  ok("reflow: two wrapped sentences give exactly two units", n == 2);
  free_sentences(s, n);

  char big[4096];
  {
    int off = 0;
    off += sprintf(big + off, "First short line.\n");
    for(int i = 0; i < 200; i++) big[off++] = 'x';
    off += sprintf(big + off, "\nmore body then it ends here.\n");
    big[off] = '\0';
  }
  s = build_sentences(big, &n);
  ok("reflow: one long line does not fragment the rest", n == 2);
  free_sentences(s, n);

  s = build_sentences("   \n\n  ", &n);
  ok("reflow: whitespace-only yields no sentences", n == 0);
  free_sentences(s, n);
}

static void test_words(void){
  word_span *w;
  int n;

  n = wrap_words("the quick brown fox", 40, &w);
  ok("words: four tokens on one line", n == 4);
  ok("words: 'the' at row0 col0 cells3",
     n == 4 && w[0].row == 0 && w[0].col == 0 && w[0].cells == 3);
  ok("words: 'quick' at row0 col4 cells5",
     n == 4 && w[1].row == 0 && w[1].col == 4 && w[1].cells == 5);
  ok("words: 'fox' at row0 col16 cells3",
     n == 4 && w[3].row == 0 && w[3].col == 16 && w[3].cells == 3);
  free(w);

  n = wrap_words("alpha beta gamma delta", 11, &w);
  ok("words: still four tokens when wrapped", n == 4);
  ok("words: wrap advances to a later row",
     n == 4 && w[n-1].row > 0);
  int okcol = 1;
  for(int i = 0; i < n; i++)
    if(w[i].col < 0 || w[i].col + w[i].cells > 11) okcol = 0;
  ok("words: every token fits within the column", okcol);
  free(w);

  n = wrap_words("cafe naive resume", 40, &w);
  ok("words: utf-8-ish tokens counted by display width",
     n == 3 && w[0].cells == 4 && w[1].cells == 5 && w[2].cells == 6);
  free(w);

  n = wrap_words("   ", 20, &w);
  ok("words: whitespace-only yields no tokens", n == 0);
  free(w);
}

static void test_input(void){
  char b1[] = "N\bNA\bAM\bME\bE";
  size_t n1 = collapse_overstrike(b1, sizeof b1 - 1);
  b1[n1] = '\0';
  ok("overstrike: bold NAME collapses to NAME", !strcmp(b1, "NAME"));

  char b2[] = "_\bf_\bi_\bl_\be";
  size_t n2 = collapse_overstrike(b2, sizeof b2 - 1);
  b2[n2] = '\0';
  ok("overstrike: underlined file collapses to file", !strcmp(b2, "file"));

  char pdf[] =
    "Running header\nBody of page one continues.\n1\n"
    "\fRunning header\nBody of page two continues here.\n2\n"
    "\fRunning header\nAnd more body on the third page.\n3\n";
  size_t np = strip_pdf_furniture(pdf, sizeof pdf - 1);
  pdf[np] = '\0';
  ok("pdf: repeated running header removed",
     strstr(pdf, "Running header") == NULL);
  ok("pdf: page-number lines removed",
     strstr(pdf, "\n1\n") == NULL && strchr(pdf, '2') == NULL);
  ok("pdf: body text kept",
     strstr(pdf, "Body of page one continues.") != NULL &&
     strstr(pdf, "third page") != NULL);
}

int main(void){
  test_wrap();
  test_split();
  test_reflow();
  test_words();
  test_input();
  printf("\n%d passed, %d failed\n", pass, fail);
  return fail != 0;
}
