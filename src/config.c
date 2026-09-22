#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"
#include "ui.h"

static int parse_bool(const char *s){
  return (!strcmp(s, "on") || !strcmp(s, "true") ||
          !strcmp(s, "yes") || !strcmp(s, "1"));
}

static int config_paths(char *file, size_t fn, char *dir, size_t dn){
  const char *xdg = getenv("XDG_CONFIG_HOME");
  const char *home = getenv("HOME");

  if(xdg && *xdg)
    snprintf(dir, dn, "%s/readeasy", xdg);
  else if(home && *home)
    snprintf(dir, dn, "%s/.config/readeasy", home);
  else
    return 0;

  snprintf(file, fn, "%s/config", dir);
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

void config_load(ui_opts *opts){
  static char voice_store[512];
  char path[512], dir[512];

  if(!config_paths(path, sizeof path, dir, sizeof dir)) return;

  FILE *f = fopen(path, "r");
  if(f == NULL) return;

  char line[512];
  while(fgets(line, sizeof line, f)){
    char *p = line;
    while(*p == ' ' || *p == '\t') p++;
    if(*p == '#' || *p == '\n' || *p == '\0') continue;

    char *key = p;
    while(*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
    if(*p) *p++ = '\0';
    while(*p == ' ' || *p == '\t') p++;

    char *val = p;
    size_t vl = strlen(val);
    while(vl > 0 && (val[vl-1] == '\n' || val[vl-1] == '\r' ||
                     val[vl-1] == ' '  || val[vl-1] == '\t')) val[--vl] = '\0';

    if(!strcmp(key, "rate")){
      char *e; long v = strtol(val, &e, 10);
      if(*val && *e == '\0' && v >= RATE_MIN && v <= RATE_MAX) opts->rate = (int)v;
    } else if(!strcmp(key, "width")){
      char *e; long v = strtol(val, &e, 10);
      if(*val && *e == '\0' && v >= WIDTH_MIN) opts->width = (int)v;
    } else if(!strcmp(key, "voice")){
      snprintf(voice_store, sizeof voice_store, "%s", val);
      opts->voice = voice_store;
    } else if(!strcmp(key, "theme")){
      int idx = ui_theme_index(val);
      if(idx >= 0) opts->theme = idx;
    } else if(!strcmp(key, "focus")){
      opts->focus = parse_bool(val);
    } else if(!strcmp(key, "word_highlight")){
      opts->word_highlight = parse_bool(val);
    } else if(!strcmp(key, "pause")){
      char *e; long v = strtol(val, &e, 10);
      if(*val && *e == '\0' && v >= 0 && v <= PAUSE_MAX) opts->pause_ms = (int)v;
    }
  }

  fclose(f);
}

int config_save(const ui_opts *opts){
  char path[512], dir[512];

  if(!config_paths(path, sizeof path, dir, sizeof dir)) return -1;
  mkdir_p(dir);

  FILE *f = fopen(path, "w");
  if(f == NULL) return -1;

  fputs("# readeasy settings (managed by the in-app menu; press , to change)\n",
        f);
  fprintf(f, "rate %d\n", opts->rate);
  if(opts->width > 0) fprintf(f, "width %d\n", opts->width);
  if(opts->voice && *opts->voice) fprintf(f, "voice %s\n", opts->voice);
  fprintf(f, "theme %s\n", ui_theme_name(opts->theme));
  fprintf(f, "focus %s\n", opts->focus ? "on" : "off");
  fprintf(f, "word_highlight %s\n", opts->word_highlight ? "on" : "off");
  if(opts->pause_ms > 0) fprintf(f, "pause %d\n", opts->pause_ms);

  fclose(f);
  return 0;
}
