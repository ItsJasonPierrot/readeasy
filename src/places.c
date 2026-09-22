#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "places.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PLACE_LINE (PATH_MAX + 256)

static char *dupstr(const char *s){
  size_t n = strlen(s) + 1;
  char *d = malloc(n);
  if(d != NULL) memcpy(d, s, n);
  return d;
}

static int places_paths(char *file, size_t fn, char *dir, size_t dn){
  const char *xdg = getenv("XDG_CONFIG_HOME");
  const char *home = getenv("HOME");

  if(xdg && *xdg)
    snprintf(dir, dn, "%s/readeasy", xdg);
  else if(home && *home)
    snprintf(dir, dn, "%s/.config/readeasy", home);
  else
    return 0;

  snprintf(file, fn, "%s/places", dir);
  return 1;
}

static void mkdir_p(const char *path){
  char tmp[600];
  snprintf(tmp, sizeof tmp, "%s", path);
  for(char *p = tmp + 1; *p; p++){
    if(*p == '/'){
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
}

static void canon(const char *path, char *out, size_t n){
  char buf[PATH_MAX];
  if(realpath(path, buf) != NULL) snprintf(out, n, "%s", buf);
  else                            snprintf(out, n, "%s", path);
}

static void strip_eol(char *s){
  size_t l = strlen(s);
  while(l > 0 && (s[l-1] == '\n' || s[l-1] == '\r')) s[--l] = '\0';
}

static const char *record_path(char *line, int *is_mark, long *idx,
                               const char **name){
  char *t1 = strchr(line, '\t');
  if(t1 == NULL) return NULL;
  *t1 = '\0';
  char *field2 = t1 + 1;
  char *t2 = strchr(field2, '\t');
  if(t2 == NULL) return NULL;
  *t2 = '\0';
  char *field3 = t2 + 1;

  char *e;
  long v = strtol(field2, &e, 10);
  if(*field2 == '\0' || *e != '\0') return NULL;
  *idx = v;

  const char *nm = "";
  char *t3 = strchr(field3, '\t');
  if(t3 != NULL){ *t3 = '\0'; nm = t3 + 1; }
  if(name != NULL) *name = nm;

  if(!strcmp(line, "place"))     *is_mark = 0;
  else if(!strcmp(line, "mark")) *is_mark = 1;
  else return NULL;

  return field3;
}

int places_load(const char *path, int *pos, bookmark **marks, int *nmarks){
  *pos = -1;
  *marks = NULL;
  *nmarks = 0;
  if(path == NULL) return 0;

  char file[512], dir[512], key[PATH_MAX];
  if(!places_paths(file, sizeof file, dir, sizeof dir)) return 0;
  canon(path, key, sizeof key);

  FILE *f = fopen(file, "r");
  if(f == NULL) return 0;

  bookmark *arr = NULL;
  int n = 0, cap = 0;

  char line[PLACE_LINE];
  while(fgets(line, sizeof line, f)){
    strip_eol(line);
    if(line[0] == '#' || line[0] == '\0') continue;

    int is_mark; long idx; const char *name;
    char *rp = (char *)record_path(line, &is_mark, &idx, &name);
    if(rp == NULL) continue;
    if(strcmp(rp, key) != 0) continue;

    if(!is_mark){
      *pos = (int)idx;
    } else {
      if(n == cap){
        int nc = cap ? cap * 2 : 8;
        bookmark *g = realloc(arr, (size_t)nc * sizeof *g);
        if(g == NULL) continue;
        arr = g;
        cap = nc;
      }
      arr[n].sent = (int)idx;
      arr[n].name = dupstr(name);
      if(arr[n].name == NULL) continue;
      n++;
    }
  }

  fclose(f);
  *marks = arr;
  *nmarks = n;
  return 0;
}

int places_save(const char *path, int pos, const bookmark *marks, int nmarks){
  if(path == NULL) return 0;

  char file[512], dir[512], key[PATH_MAX];
  if(!places_paths(file, sizeof file, dir, sizeof dir)) return -1;
  canon(path, key, sizeof key);

  char **keep = NULL;
  int nkeep = 0, capk = 0;

  FILE *f = fopen(file, "r");
  if(f != NULL){
    char line[PLACE_LINE], copy[PLACE_LINE];
    while(fgets(line, sizeof line, f)){
      strip_eol(line);
      if(line[0] == '#' || line[0] == '\0') continue;

      snprintf(copy, sizeof copy, "%s", line);
      int is_mark; long idx;
      char *rp = (char *)record_path(copy, &is_mark, &idx, NULL);
      if(rp == NULL) continue;
      if(strcmp(rp, key) == 0) continue;

      if(nkeep == capk){
        int nc = capk ? capk * 2 : 8;
        char **g = realloc(keep, (size_t)nc * sizeof *g);
        if(g == NULL){ continue; }
        keep = g;
        capk = nc;
      }
      keep[nkeep] = dupstr(line);
      if(keep[nkeep] != NULL) nkeep++;
    }
    fclose(f);
  }

  mkdir_p(dir);

  FILE *o = fopen(file, "w");
  if(o == NULL){
    for(int i = 0; i < nkeep; i++) free(keep[i]);
    free(keep);
    return -1;
  }

  fputs("# readeasy reading positions and bookmarks (managed automatically)\n",
        o);
  for(int i = 0; i < nkeep; i++) fprintf(o, "%s\n", keep[i]);
  if(pos > 0) fprintf(o, "place\t%d\t%s\n", pos, key);
  for(int i = 0; i < nmarks; i++)
    fprintf(o, "mark\t%d\t%s\t%s\n", marks[i].sent, key, marks[i].name);

  fclose(o);
  for(int i = 0; i < nkeep; i++) free(keep[i]);
  free(keep);
  return 0;
}

void places_free(bookmark *marks, int nmarks){
  if(marks == NULL) return;
  for(int i = 0; i < nmarks; i++) free(marks[i].name);
  free(marks);
}
