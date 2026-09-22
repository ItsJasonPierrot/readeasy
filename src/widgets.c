#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include "widgets.h"
#include "ui.h"

int prompt_search(WINDOW *sbar, int cols, char *buf, int cap){
  int len = 0;
  buf[0] = '\0';
  curs_set(1);
  for(;;){
    werase(sbar);
    mvwprintw(sbar, 0, 1, "Search: %.*s", cols - 10, buf);
    wrefresh(sbar);
    int c = getch();
    if(c == '\n' || c == '\r' || c == KEY_ENTER){ curs_set(0); return len > 0; }
    if(c == 27){ curs_set(0); return 0; }
    if(c == KEY_BACKSPACE || c == 127 || c == 8){
      if(len > 0) buf[--len] = '\0';
    } else if(c >= 32 && c < 127 && len < cap - 1){
      buf[len++] = (char)c;
      buf[len] = '\0';
    }
  }
}

int prompt_text(WINDOW *sbar, int cols, const char *label, char *buf, int cap){
  int len = (int)strlen(buf);
  if(len > cap - 1){ len = cap - 1; buf[len] = '\0'; }
  int llen = (int)strlen(label);
  int room = cols - llen - 4;
  if(room < 1) room = 1;
  curs_set(1);
  for(;;){
    werase(sbar);
    mvwprintw(sbar, 0, 1, "%s: %.*s", label, room, buf);
    wrefresh(sbar);
    int c = getch();
    if(c == '\n' || c == '\r' || c == KEY_ENTER){ curs_set(0); return 1; }
    if(c == 27){ curs_set(0); return 0; }
    if(c == KEY_BACKSPACE || c == 127 || c == 8){
      if(len > 0){
        len--;
        while(len > 0 && ((unsigned char)buf[len] & 0xC0) == 0x80) len--;
        buf[len] = '\0';
      }
    } else if(c >= 32 && c < 127 && len < cap - 1){
      buf[len++] = (char)c;
      buf[len] = '\0';
    }
  }
}

void show_help(int rows, int cols, short color_pair){
  static const char *keys[] = {
    "Space        play / pause",
    "r            replay this sentence",
    "Up / Down    move one sentence",
    "PgUp / PgDn  move a screenful",
    "Home / End   first / last (g / G)",
    "/  n  N      search / next / previous",
    "o            outline (jump by heading)",
    "m            bookmark this sentence",
    "'            go to a bookmark",
    "+  /  -      read faster / slower",
    "[  /  ]      narrow / widen column",
    "f            focus mode (dim the rest)",
    "w            word highlight on / off",
    "b            bionic emphasis on / off",
    "t            cycle color theme",
    ",            settings menu",
    "?            this help",
    "q            quit",
  };
  int nkeys = (int)(sizeof keys / sizeof keys[0]);

  int h = nkeys + 5;
  int w = 46;
  if(h > rows) h = rows;
  if(w > cols) w = cols;
  int y = (rows - h) / 2, x = (cols - w) / 2;
  if(y < 0) y = 0;
  if(x < 0) x = 0;

  WINDOW *win = newwin(h, w, y, x);
  if(win == NULL) return;
  keypad(win, TRUE);
  wtimeout(win, -1);
  wbkgd(win, COLOR_PAIR(color_pair));

  werase(win);
  box(win, 0, 0);
  mvwprintw(win, 1, 2, "readeasy - keys");
  for(int i = 0; i < nkeys && 3 + i < h - 2; i++)
    mvwprintw(win, 3 + i, 3, "%-*.*s", w - 5, w - 5, keys[i]);
  mvwprintw(win, h - 2, 2, "%-*.*s", w - 4, w - 4, "press any key to close");
  wrefresh(win);

  wgetch(win);
  delwin(win);
}

int list_picker(int rows, int cols, const char *title,
                char **items, int n, int start, short color_pair){
  int h = n + 4;
  if(h > rows - 2) h = rows - 2;
  if(h < 5) h = 5;
  int w = 44;
  if(w > cols - 2) w = cols - 2;
  if(w < 20) w = 20;
  int y = (rows - h) / 2, x = (cols - w) / 2;
  if(y < 0) y = 0;
  if(x < 0) x = 0;

  WINDOW *win = newwin(h, w, y, x);
  if(win == NULL) return -1;
  keypad(win, TRUE);
  wtimeout(win, -1);
  wbkgd(win, COLOR_PAIR(color_pair));

  int view = h - 4;
  if(view < 1) view = 1;
  int sel = start;
  if(sel < 0) sel = 0;
  if(sel >= n) sel = n - 1;
  int off = 0, result = -1;

  for(;;){
    if(sel < off) off = sel;
    if(sel >= off + view) off = sel - view + 1;

    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 2, "%-*.*s", w - 4, w - 4, title);
    for(int i = 0; i < view && off + i < n; i++){
      int idx = off + i;
      if(idx == sel) wattron(win, A_REVERSE);
      mvwprintw(win, 2 + i, 2, "%-*.*s", w - 4, w - 4, items[idx]);
      if(idx == sel) wattroff(win, A_REVERSE);
    }
    mvwprintw(win, h - 2, 2, "%-*.*s", w - 4, w - 4,
              "up/dn  Enter select  Esc cancel");
    wrefresh(win);

    int c = wgetch(win);
    if(c == KEY_UP || c == 'k'){ if(sel > 0) sel--; }
    else if(c == KEY_DOWN || c == 'j'){ if(sel < n - 1) sel++; }
    else if(c == KEY_NPAGE){ sel += view; if(sel >= n) sel = n - 1; }
    else if(c == KEY_PPAGE){ sel -= view; if(sel < 0) sel = 0; }
    else if(c == KEY_HOME || c == 'g'){ sel = 0; }
    else if(c == KEY_END || c == 'G'){ sel = n - 1; }
    else if(c == '\n' || c == '\r' || c == KEY_ENTER){ result = sel; break; }
    else if(c == 27 || c == 'q'){ result = -1; break; }
  }
  delwin(win);
  return result;
}

void draw_settings(WINDOW *w, int sel, int rate, int width, int cols,
                   int theme_idx, int focus, int word_on,
                   const char *voice, int pause_ms, int bionic, int saved){
  int H, W;
  getmaxyx(w, H, W);
  (void)cols;

  char wbuf[24];
  if(width > 0) snprintf(wbuf, sizeof wbuf, "%d cols", width);
  else          snprintf(wbuf, sizeof wbuf, "full");

  const char *labels[8] = { "Theme", "Speed", "Width", "Focus",
                            "Word highlight", "Voice", "Pause", "Bionic" };
  char vals[8][40];
  snprintf(vals[0], sizeof vals[0], "%s", ui_theme_name(theme_idx));
  snprintf(vals[1], sizeof vals[1], "%d wpm", rate);
  snprintf(vals[2], sizeof vals[2], "%s", wbuf);
  snprintf(vals[3], sizeof vals[3], "%s", focus ? "on" : "off");
  snprintf(vals[4], sizeof vals[4], "%s", word_on ? "on" : "off");
  snprintf(vals[5], sizeof vals[5], "%s", (voice && *voice) ? voice : "(default)");
  if(pause_ms > 0) snprintf(vals[6], sizeof vals[6], "%d ms", pause_ms);
  else             snprintf(vals[6], sizeof vals[6], "off");
  snprintf(vals[7], sizeof vals[7], "%s", bionic ? "on" : "off");

  werase(w);
  box(w, 0, 0);
  mvwprintw(w, 1, 2, "readeasy settings");

  for(int i = 0; i < 8; i++){
    if(i == sel) wattron(w, A_REVERSE);
    mvwprintw(w, 3 + i, 2, " %-14s  < %-18.18s > ", labels[i], vals[i]);
    if(i == sel) wattroff(w, A_REVERSE);
  }
  if(sel == 8) wattron(w, A_REVERSE);
  mvwprintw(w, 12, 2, " %-38s", "Save settings to config");
  if(sel == 8) wattroff(w, A_REVERSE);

  if(saved == 1)       mvwprintw(w, 13, 2, "%-40.40s", "Saved.");
  else if(saved == -1) mvwprintw(w, 13, 2, "%-40.40s", "Could not save config.");
  else                 mvwprintw(w, 13, 2, "%-40.40s", "");

  mvwprintw(w, H - 2, 2, "%.*s", W - 4,
            "up/dn pick  left/right change  s save  Esc close");
  wrefresh(w);
}
