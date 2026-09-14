#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <locale.h>
#include <time.h>
#include <ctype.h>
#include "speech.h"
#include "reflow.h"
#include "ui.h"
#include "config.h"

enum { STOPPED, SYNTH, PLAY };

#define RATE_STEP 20

typedef struct {
  const char *name;
  int fg[3];
  int bg[3];
  short fg_basic;
  short bg_basic;
} theme_t;

static const theme_t themes[] = {
  { "none",     {0,0,0},          {0,0,0},          0,            0           },
  { "blue",     {1000,780,560},   {60,100,250},     COLOR_YELLOW, COLOR_BLUE  },
  { "cream",    {210,140,70},     {1000,975,910},   COLOR_BLACK,  COLOR_WHITE },
  { "contrast", {1000,1000,1000}, {0,0,0},          COLOR_WHITE,  COLOR_BLACK },
  { "dark",     {820,840,880},    {120,130,150},    COLOR_WHITE,  COLOR_BLACK },
};
static const int n_themes = (int)(sizeof themes / sizeof themes[0]);

int ui_theme_count(void){ return n_themes; }

const char *ui_theme_name(int i){
  return (i >= 0 && i < n_themes) ? themes[i].name : "";
}

int ui_theme_index(const char *name){
  for(int i = 0; i < n_themes; i++)
    if(strcmp(name, themes[i].name) == 0) return i;
  return -1;
}

static volatile sig_atomic_t curses_active = 0;
static short color_pair = 1;
static attr_t normal_attr = A_NORMAL;
static int text_width = 0;
static int text_col = 0;
static int line_gap = 0;
static pid_t synth_pid = 0;
static pid_t play_pid = 0;
static char audio_dir[] = "/tmp/readeasy.XXXXXX";
static char audio_a[64];
static char audio_b[64];
static int audio_ok = 0;

static int kara_on = 1;
static word_span *kspan = NULL;
static int knspan = 0;
static int ktotal = 0;
static int kidx = -1;
static double kt0 = 0.0;
static double kdur = 0.0;
static int kplaying = 0;

static void cleanup(void){
  if(curses_active){
    endwin();
    curses_active = 0;
  }
  if(play_pid > 0){
    kill(play_pid, SIGTERM);
    play_pid = 0;
  }
  if(synth_pid > 0){
    kill(synth_pid, SIGTERM);
    synth_pid = 0;
  }
  if(audio_ok){
    unlink(audio_a);
    unlink(audio_b);
    rmdir(audio_dir);
    audio_ok = 0;
  }
}

static void on_signal(int sig){
  cleanup();
  signal(sig, SIG_DFL);
  raise(sig);
}

static WINDOW *build_pad(char **sent, int nsent, int rows, int cols,
                         int *sent_row, int *content_rows);
static int page_to(const int *sent_row, int nsent, int cur, int delta);
static void set_layout(int width, int cols);
static void apply_theme(int idx);
static void apply_base(WINDOW *pad, const int *sent_row, int nsent);
static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr);
static void goto_line(WINDOW *pad, const int *row, int content_rows,
                      int rows, int cols, int *top, int old, int cur);
static void stop_audio(pid_t *synth_pid, pid_t *play_pid,
                       int *synth_i, int *ready);
static void draw_status(WINDOW *sbar, const char *name, int cur, int nsent,
                        int playing, int rate, int words, int cols);
static void kara_build(char **sent, int cur);
static void kara_frame(WINDOW *pad, const int *sent_row, int cur);
static void kara_begin(WINDOW *pad, char **sent, const int *sent_row, int cur,
                       const char *pfile, int rate, int top, int view_rows,
                       int cols);
static void kara_advance(WINDOW *pad, const int *sent_row, int cur,
                         int top, int view_rows, int cols);
static void kara_stop(void);
static void draw_settings(WINDOW *w, int sel, int rate, int width, int cols,
                          int theme_idx, int focus, int word_on,
                          const char *voice, int saved);
static int list_picker(int rows, int cols, const char *title,
                       char **items, int n, int start);
static void show_help(int rows, int cols);
static int find_match(char **sent, int nsent, int first, const char *q, int dir);
static int prompt_search(WINDOW *sbar, int cols, char *buf, int cap);
static int is_heading_sentence(const char *s);

int run_ui(char *text, const char *name, const ui_opts *opts){
  int rows, cols, view_rows, has_status;
  int content_rows;
  int top = 0;
  int cur = 0;
  int nsent = 0;
  int character;
  int *sent_row = NULL;
  char **sent;
  WINDOW *pad, *sbar = NULL;

  int menu_open = 0, menu_sel = 0, menu_saved = 0, vsel = -1, nvoices = 0;
  WINDOW *menu_win = NULL;
  char **voices = NULL;

  char query[128] = "";
  int have_query = 0;

  int astate = STOPPED;
  int synth_i = -1;
  int ready = -1;
  int rate = opts->rate;
  int focus = opts->focus;
  int width = opts->width;
  int theme_idx = opts->theme;
  const char *voice = opts->voice;
  char *pcur = audio_a, *pnext = audio_b, *pswap;

  kara_on = opts->word_highlight ? 1 : 0;

  if(rate < RATE_MIN) rate = RATE_MIN;
  if(rate > RATE_MAX) rate = RATE_MAX;
  normal_attr = focus ? A_DIM : A_NORMAL;

  sent = build_sentences(text, &nsent);

  int words = 0;
  {
    int in_word = 0;
    for(const char *p = text; *p; p++){
      if(*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r') in_word = 0;
      else if(!in_word){ in_word = 1; words++; }
    }
  }

  setlocale(LC_ALL, "");
  initscr();
  curses_active = 1;
  signal(SIGINT,  on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGHUP,  on_signal);
  signal(SIGQUIT, on_signal);
  atexit(cleanup);
  start_color();
  apply_theme(theme_idx);
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  curs_set(0);

  define_key("\033[A", KEY_UP);
  define_key("\033[B", KEY_DOWN);
  define_key("\033OA", KEY_UP);
  define_key("\033OB", KEY_DOWN);
  define_key("\033[5~", KEY_PPAGE);
  define_key("\033[6~", KEY_NPAGE);
  define_key("\033[H",  KEY_HOME);
  define_key("\033[F",  KEY_END);
  define_key("\033[1~", KEY_HOME);
  define_key("\033[4~", KEY_END);

  getmaxyx(stdscr, rows, cols);
  has_status = rows > 1;
  view_rows = has_status ? rows - 1 : rows;
  set_layout(width, cols);

  wbkgd(stdscr, COLOR_PAIR(color_pair));
  clear();
  refresh();

  audio_ok = (mkdtemp(audio_dir) != NULL) && (sent != NULL);
  if(audio_ok){
    snprintf(audio_a, sizeof audio_a, "%s/a.%s", audio_dir, AUDIO_EXT);
    snprintf(audio_b, sizeof audio_b, "%s/b.%s", audio_dir, AUDIO_EXT);
  }

  sent_row = malloc((size_t)(nsent + 1) * sizeof(int));
  if(sent_row == NULL){
    free_sentences(sent, nsent);
    cleanup();
    return 1;
  }

  pad = build_pad(sent, nsent, rows, cols, sent_row, &content_rows);
  if(pad == NULL){
    free(sent_row);
    free_sentences(sent, nsent);
    cleanup();
    return 1;
  }

  if(has_status){
    sbar = newwin(1, cols, rows - 1, 0);
    if(sbar) wbkgd(sbar, COLOR_PAIR(color_pair) | A_REVERSE);
  }

  if(focus && nsent > 0) apply_base(pad, sent_row, nsent);
  if(nsent > 0)
    goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
  else
    prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
  if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, rate, words, cols);

  while(1){
    timeout(astate == STOPPED ? -1 : 100);
    character = getch();

    if(menu_open){
      int repaint_pad = 0, dir = 0;

      if(character == KEY_UP || character == 'k')        menu_sel = (menu_sel + 6) % 7;
      else if(character == KEY_DOWN || character == 'j') menu_sel = (menu_sel + 1) % 7;
      else if(character == KEY_LEFT  || character == 'h') dir = -1;
      else if(character == KEY_RIGHT || character == 'l') dir = 1;
      else if(character == 's' || character == 'S'){
        ui_opts co = { .rate = rate, .voice = voice, .theme = theme_idx,
                       .focus = focus, .width = width, .word_highlight = kara_on };
        menu_saved = (config_save(&co) == 0) ? 1 : -1;
      }
      else if(character == '\n' || character == '\r' || character == KEY_ENTER){
        if(menu_sel == 6){
          ui_opts co = { .rate = rate, .voice = voice, .theme = theme_idx,
                         .focus = focus, .width = width, .word_highlight = kara_on };
          menu_saved = (config_save(&co) == 0) ? 1 : -1;
        } else if(menu_sel == 5){
          int pn = nvoices + 1;
          char **items = malloc((size_t)pn * sizeof(char *));
          if(items != NULL){
            items[0] = (char *)"(system default)";
            for(int i = 0; i < nvoices; i++) items[i+1] = voices[i];
            int chosen = list_picker(rows, cols, "Choose a voice", items, pn, vsel + 1);
            free(items);
            if(chosen >= 0){
              vsel = chosen - 1;
              voice = (vsel >= 0) ? voices[vsel] : NULL;
            }
            menu_saved = 0;
            clearok(curscr, TRUE);
            touchwin(stdscr);
            refresh();
            repaint_pad = 1;
          }
        } else dir = 1;
      }
      else if(character == 27 || character == ','){
        delwin(menu_win);
        menu_win = NULL;
        menu_open = 0;
        clearok(curscr, TRUE);
        touchwin(stdscr);
        refresh();
        if(focus && nsent > 0) apply_base(pad, sent_row, nsent);
        if(nsent > 0)
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
        else
          prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
        if(sbar) draw_status(sbar, name, cur, nsent, 0, rate, words, cols);
        continue;
      }

      if(dir != 0){
        menu_saved = 0;
        switch(menu_sel){
          case 0:
            theme_idx = (theme_idx + dir + n_themes) % n_themes;
            apply_theme(theme_idx);
            wbkgd(stdscr, COLOR_PAIR(color_pair));
            if(sbar) wbkgd(sbar, COLOR_PAIR(color_pair) | A_REVERSE);
            wbkgd(pad, COLOR_PAIR(color_pair));
            wbkgd(menu_win, COLOR_PAIR(color_pair));
            if(nsent > 0) apply_base(pad, sent_row, nsent);
            clearok(curscr, TRUE);
            touchwin(stdscr);
            refresh();
            repaint_pad = 1;
            break;
          case 1:
            rate += dir * RATE_STEP;
            if(rate > RATE_MAX) rate = RATE_MAX;
            if(rate < RATE_MIN) rate = RATE_MIN;
            break;
          case 2:
            if(dir < 0){
              if(width <= 0 || width > cols) width = cols;
              width -= WIDTH_STEP;
              if(width < WIDTH_MIN) width = WIDTH_MIN;
            } else if(width > 0){
              width += WIDTH_STEP;
              if(width >= cols) width = 0;
            }
            set_layout(width, cols);
            {
              int off = (nsent > 0) ? sent_row[cur] - top : 0;
              WINDOW *np = build_pad(sent, nsent, rows, cols, sent_row, &content_rows);
              if(np != NULL){
                delwin(pad);
                pad = np;
                if(nsent > 0) top = sent_row[cur] - off;
              }
            }
            if(focus && nsent > 0) apply_base(pad, sent_row, nsent);
            repaint_pad = 1;
            break;
          case 3:
            focus = !focus;
            normal_attr = focus ? A_DIM : A_NORMAL;
            if(nsent > 0) apply_base(pad, sent_row, nsent);
            repaint_pad = 1;
            break;
          case 4:
            kara_on = !kara_on;
            break;
          case 5:
            if(nvoices > 0){
              int total = nvoices + 1;
              int p = (((vsel + 1 + dir) % total) + total) % total;
              vsel = p - 1;
              voice = (vsel >= 0) ? voices[vsel] : NULL;
            }
            break;
        }
      }

      if(repaint_pad){
        if(nsent > 0)
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
        else
          prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
      }
      if(menu_win)
        draw_settings(menu_win, menu_sel, rate, width, cols, theme_idx, focus,
                      kara_on, voice, menu_saved);
      continue;
    }

    if(character == KEY_RESIZE || character == 12){
      getmaxyx(stdscr, rows, cols);
      has_status = rows > 1;
      view_rows = has_status ? rows - 1 : rows;
      set_layout(width, cols);
      if(sbar){ delwin(sbar); sbar = NULL; }
      if(has_status){
        sbar = newwin(1, cols, rows - 1, 0);
        if(sbar) wbkgd(sbar, COLOR_PAIR(color_pair) | A_REVERSE);
      }
      flushinp();
      wbkgd(stdscr, COLOR_PAIR(color_pair));
      clearok(curscr, TRUE);
      touchwin(stdscr);
      refresh();
      if(nsent > 0 && (cols != getmaxx(pad) || getmaxy(pad) < rows + 1)){
        int off = sent_row[cur] - top;
        WINDOW *np = build_pad(sent, nsent, rows, cols, sent_row, &content_rows);
        if(np != NULL){
          delwin(pad);
          pad = np;
          top = sent_row[cur] - off;
        }
      }
      if(focus && nsent > 0) apply_base(pad, sent_row, nsent);
      if(nsent > 0)
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
      else
        prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
      if(kplaying && nsent > 0){
        kara_build(sent, cur);
        if(kara_on){
          kara_frame(pad, sent_row, cur);
          prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
        }
      }
      if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, rate, words, cols);
      continue;
    }

    if(nsent > 0 && (character == KEY_DOWN  || character == KEY_UP    ||
                     character == KEY_NPAGE || character == KEY_PPAGE ||
                     character == KEY_HOME  || character == KEY_END   ||
                     character == 'g'       || character == 'G')){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        astate = STOPPED;
      }
      int old = cur;
      switch(character){
        case KEY_DOWN:  if(cur < nsent - 1) cur++;                       break;
        case KEY_UP:    if(cur > 0) cur--;                               break;
        case KEY_NPAGE: cur = page_to(sent_row, nsent, cur, view_rows);  break;
        case KEY_PPAGE: cur = page_to(sent_row, nsent, cur, -view_rows); break;
        case KEY_HOME:
        case 'g':       cur = 0;                                         break;
        case KEY_END:
        case 'G':       cur = nsent - 1;                                 break;
      }
      goto_line(pad, sent_row, content_rows, view_rows, cols, &top, old, cur);
    }

    if(character == ' ' && nsent > 0 && audio_ok){
      if(astate == STOPPED){
        if(synth_to_file(sent[cur], pcur, rate, voice, &synth_pid) == 0){
          synth_i = cur;
          astate = SYNTH;
        }
      } else {
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        paint_line(pad, sent_row, cur, A_REVERSE);
        prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
        astate = STOPPED;
      }
    }

    if(character == 'q'){
      stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
      break;
    }

    if(character == '+' || character == '=' ||
       character == '-' || character == '_'){
      int old_rate = rate;
      if(character == '+' || character == '='){
        rate += RATE_STEP;
        if(rate > RATE_MAX) rate = RATE_MAX;
      } else {
        rate -= RATE_STEP;
        if(rate < RATE_MIN) rate = RATE_MIN;
      }
      if(rate != old_rate && astate != STOPPED && nsent > 0 && audio_ok){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        if(synth_to_file(sent[cur], pcur, rate, voice, &synth_pid) == 0){
          synth_i = cur;
          astate = SYNTH;
        } else {
          astate = STOPPED;
          paint_line(pad, sent_row, cur, A_REVERSE);
          prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
        }
      }
    }

    if(character == 'f'){
      focus = !focus;
      normal_attr = focus ? A_DIM : A_NORMAL;
      if(nsent > 0){
        apply_base(pad, sent_row, nsent);
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
        if(kplaying && kara_on){
          kara_frame(pad, sent_row, cur);
          prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
        }
      }
    }

    if(character == '[' || character == ']'){
      if(character == '['){
        if(width <= 0 || width > cols) width = cols;
        width -= WIDTH_STEP;
        if(width < WIDTH_MIN) width = WIDTH_MIN;
      } else if(width > 0){
        width += WIDTH_STEP;
        if(width >= cols) width = 0;
      }
      set_layout(width, cols);
      if(nsent > 0){
        int off = sent_row[cur] - top;
        WINDOW *np = build_pad(sent, nsent, rows, cols, sent_row, &content_rows);
        if(np != NULL){
          delwin(pad);
          pad = np;
          top = sent_row[cur] - off;
        }
        if(focus) apply_base(pad, sent_row, nsent);
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
        if(kplaying){
          kara_build(sent, cur);
          if(kara_on){
            kara_frame(pad, sent_row, cur);
            prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
          }
        }
      }
    }

    if(character == 't'){
      theme_idx = (theme_idx + 1) % n_themes;
      apply_theme(theme_idx);
      wbkgd(stdscr, COLOR_PAIR(color_pair));
      if(sbar) wbkgd(sbar, COLOR_PAIR(color_pair) | A_REVERSE);
      wbkgd(pad, COLOR_PAIR(color_pair));
      if(nsent > 0) apply_base(pad, sent_row, nsent);
      clearok(curscr, TRUE);
      touchwin(stdscr);
      refresh();
      if(nsent > 0)
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
      else
        prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
      if(kplaying && kara_on && nsent > 0){
        kara_frame(pad, sent_row, cur);
        prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
      }
      if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, rate, words, cols);
    }

    if(character == 'w'){
      kara_on = !kara_on;
      if(kplaying && nsent > 0){
        if(kara_on) kara_frame(pad, sent_row, cur);
        else paint_line(pad, sent_row, cur, A_REVERSE);
        prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
      }
    }

    if(character == ','){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        astate = STOPPED;
      }
      if(voices == NULL) voices = list_voices(&nvoices);
      vsel = -1;
      if(voice != NULL)
        for(int i = 0; i < nvoices; i++)
          if(strcmp(voice, voices[i]) == 0){ vsel = i; break; }

      int menu_h = 14, menu_w = 54;
      if(menu_h > rows) menu_h = rows;
      if(menu_w > cols) menu_w = cols;
      int my = (rows - menu_h) / 2, mx = (cols - menu_w) / 2;
      if(my < 0) my = 0;
      if(mx < 0) mx = 0;

      menu_win = newwin(menu_h, menu_w, my, mx);
      if(menu_win != NULL){
        keypad(menu_win, TRUE);
        wbkgd(menu_win, COLOR_PAIR(color_pair));
        menu_open = 1;
        menu_sel = 0;
        menu_saved = 0;
        if(nsent > 0)
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
        else
          prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
        if(sbar) draw_status(sbar, name, cur, nsent, 0, rate, words, cols);
        draw_settings(menu_win, menu_sel, rate, width, cols, theme_idx, focus,
                      kara_on, voice, menu_saved);
      }
      continue;
    }

    if(character == '?'){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        astate = STOPPED;
      }
      show_help(rows, cols);
      clearok(curscr, TRUE);
      touchwin(stdscr);
      refresh();
      if(focus && nsent > 0) apply_base(pad, sent_row, nsent);
      if(nsent > 0)
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
      else
        prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
      if(sbar) draw_status(sbar, name, cur, nsent, 0, rate, words, cols);
      continue;
    }

    if(character == '/' && sbar != NULL && nsent > 0){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        astate = STOPPED;
      }
      timeout(-1);
      char q[128];
      if(prompt_search(sbar, cols, q, sizeof q)){
        snprintf(query, sizeof query, "%s", q);
        have_query = 1;
        int m = find_match(sent, nsent, cur, query, 1);
        if(m >= 0){
          int old = cur;
          cur = m;
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, old, cur);
          draw_status(sbar, name, cur, nsent, 0, rate, words, cols);
        } else {
          werase(sbar);
          mvwprintw(sbar, 0, 1, "Not found: %.*s", cols - 13, query);
          wrefresh(sbar);
        }
      } else {
        draw_status(sbar, name, cur, nsent, 0, rate, words, cols);
      }
      continue;
    }

    if((character == 'n' || character == 'N') && have_query &&
       sbar != NULL && nsent > 0){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        astate = STOPPED;
      }
      int dir = (character == 'n') ? 1 : -1;
      int m = find_match(sent, nsent, cur + dir, query, dir);
      if(m >= 0){
        int old = cur;
        cur = m;
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, old, cur);
        draw_status(sbar, name, cur, nsent, 0, rate, words, cols);
      } else {
        werase(sbar);
        mvwprintw(sbar, 0, 1, "Not found: %.*s", cols - 13, query);
        wrefresh(sbar);
      }
      continue;
    }

    if((character == 'o' || character == 'O') && nsent > 0){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        kara_stop();
        astate = STOPPED;
      }
      int nout = 0;
      for(int i = 0; i < nsent; i++)
        if(is_heading_sentence(sent[i])) nout++;

      if(nout == 0){
        if(sbar){
          werase(sbar);
          mvwprintw(sbar, 0, 1, "%s", "No headings found");
          wrefresh(sbar);
        }
        continue;
      }

      char **items = malloc((size_t)nout * sizeof(char *));
      int *idx = malloc((size_t)nout * sizeof(int));
      if(items != NULL && idx != NULL){
        int j = 0, startsel = 0;
        for(int i = 0; i < nsent; i++)
          if(is_heading_sentence(sent[i])){
            items[j] = sent[i];
            idx[j] = i;
            if(i <= cur) startsel = j;
            j++;
          }
        int chosen = list_picker(rows, cols, "Outline", items, nout, startsel);
        clearok(curscr, TRUE);
        touchwin(stdscr);
        refresh();
        if(chosen >= 0){
          int old = cur;
          cur = idx[chosen];
          if(focus) apply_base(pad, sent_row, nsent);
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, old, cur);
        } else {
          if(focus) apply_base(pad, sent_row, nsent);
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
        }
        if(sbar) draw_status(sbar, name, cur, nsent, 0, rate, words, cols);
      }
      free(items);
      free(idx);
      continue;
    }

    if(astate == SYNTH){
      if(waitpid(synth_pid, NULL, WNOHANG) > 0){
        synth_i = -1;
        if(play_file(pcur, &play_pid) == 0){
          astate = PLAY;
          kara_begin(pad, sent, sent_row, cur, pcur, rate, top, view_rows, cols);
          if(cur + 1 < nsent &&
             synth_to_file(sent[cur+1], pnext, rate, voice, &synth_pid) == 0){
            synth_i = cur + 1;
            ready = -1;
          }
        } else {
          astate = STOPPED;
        }
      }
    } else if(astate == PLAY){
      if(synth_i >= 0 && waitpid(synth_pid, NULL, WNOHANG) > 0){
        ready = synth_i;
        synth_i = -1;
      }
      if(waitpid(play_pid, NULL, WNOHANG) > 0){
        if(cur + 1 >= nsent){
          kara_stop();
          paint_line(pad, sent_row, cur, A_REVERSE);
          prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
          astate = STOPPED;
        } else {
          kara_stop();
          int old = cur;
          cur++;
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, old, cur);
          pswap = pcur; pcur = pnext; pnext = pswap;
          if(ready == cur){
            ready = -1;
            if(play_file(pcur, &play_pid) == 0){
              astate = PLAY;
              kara_begin(pad, sent, sent_row, cur, pcur, rate, top, view_rows, cols);
              if(cur + 1 < nsent &&
                 synth_to_file(sent[cur+1], pnext, rate, voice, &synth_pid) == 0){
                synth_i = cur + 1;
              }
            } else {
              astate = STOPPED;
            }
          } else if(synth_i == cur){
            astate = SYNTH;
          } else if(synth_to_file(sent[cur], pcur, rate, voice, &synth_pid) == 0){
            synth_i = cur;
            astate = SYNTH;
          } else {
            astate = STOPPED;
          }
        }
      }
    }
    if(astate == PLAY)
      kara_advance(pad, sent_row, cur, top, view_rows, cols);
    if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, rate, words, cols);
  }

  if(menu_win) delwin(menu_win);
  free_voices(voices, nvoices);
  delwin(pad);
  if(sbar) delwin(sbar);
  free(sent_row);
  free(kspan);
  kspan = NULL;
  free_sentences(sent, nsent);
  cleanup();
  return 0;
}

static void draw_settings(WINDOW *w, int sel, int rate, int width, int cols,
                          int theme_idx, int focus, int word_on,
                          const char *voice, int saved){
  int H, W;
  getmaxyx(w, H, W);
  (void)cols;

  char wbuf[24];
  if(width > 0) snprintf(wbuf, sizeof wbuf, "%d cols", width);
  else          snprintf(wbuf, sizeof wbuf, "full");

  const char *labels[6] = { "Theme", "Speed", "Width",
                            "Focus", "Word highlight", "Voice" };
  char vals[6][40];
  snprintf(vals[0], sizeof vals[0], "%s", ui_theme_name(theme_idx));
  snprintf(vals[1], sizeof vals[1], "%d wpm", rate);
  snprintf(vals[2], sizeof vals[2], "%s", wbuf);
  snprintf(vals[3], sizeof vals[3], "%s", focus ? "on" : "off");
  snprintf(vals[4], sizeof vals[4], "%s", word_on ? "on" : "off");
  snprintf(vals[5], sizeof vals[5], "%s", (voice && *voice) ? voice : "(default)");

  werase(w);
  box(w, 0, 0);
  mvwprintw(w, 1, 2, "readeasy settings");

  for(int i = 0; i < 6; i++){
    if(i == sel) wattron(w, A_REVERSE);
    mvwprintw(w, 3 + i, 2, " %-14s  < %-18.18s > ", labels[i], vals[i]);
    if(i == sel) wattroff(w, A_REVERSE);
  }
  if(sel == 6) wattron(w, A_REVERSE);
  mvwprintw(w, 10, 2, " %-38s", "Save settings to config");
  if(sel == 6) wattroff(w, A_REVERSE);

  if(saved == 1)       mvwprintw(w, 11, 2, "%-40.40s", "Saved.");
  else if(saved == -1) mvwprintw(w, 11, 2, "%-40.40s", "Could not save config.");
  else                 mvwprintw(w, 11, 2, "%-40.40s", "");

  mvwprintw(w, H - 2, 2, "%.*s", W - 4,
            "up/dn pick  left/right change  s save  Esc close");
  wrefresh(w);
}

static const char *ci_strstr(const char *hay, const char *needle){
  if(*needle == '\0') return hay;
  for(; *hay; hay++){
    const char *h = hay, *n = needle;
    while(*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)){
      h++;
      n++;
    }
    if(*n == '\0') return hay;
  }
  return NULL;
}

static int is_heading_sentence(const char *s){
  size_t len = strlen(s);
  if(len == 0 || len > 60) return 0;

  int has_alpha = 0, has_lower = 0;
  for(size_t i = 0; i < len; i++){
    unsigned char c = (unsigned char)s[i];
    if(c >= 'A' && c <= 'Z') has_alpha = 1;
    else if(c >= 'a' && c <= 'z'){ has_alpha = 1; has_lower = 1; }
  }
  if(!has_alpha) return 0;
  if(!has_lower) return 1;

  char last = s[len-1];
  return last != '.' && last != '!' && last != '?' &&
         last != ':' && last != ';' && last != ',' && last != '"';
}

static int find_match(char **sent, int nsent, int first, const char *q, int dir){
  if(nsent <= 0) return -1;
  int i = ((first % nsent) + nsent) % nsent;
  for(int k = 0; k < nsent; k++){
    if(ci_strstr(sent[i], q) != NULL) return i;
    i = ((i + dir) % nsent + nsent) % nsent;
  }
  return -1;
}

static int prompt_search(WINDOW *sbar, int cols, char *buf, int cap){
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

static void show_help(int rows, int cols){
  static const char *keys[] = {
    "Space        play / pause",
    "Up / Down    move one sentence",
    "PgUp / PgDn  move a screenful",
    "Home / End   first / last (g / G)",
    "/  n  N      search / next / previous",
    "o            outline (jump by heading)",
    "+  /  -      read faster / slower",
    "[  /  ]      narrow / widen column",
    "f            focus mode (dim the rest)",
    "w            word highlight on / off",
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

static int list_picker(int rows, int cols, const char *title,
                       char **items, int n, int start){
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

static WINDOW *build_pad(char **sent, int nsent, int rows, int cols,
                         int *sent_row, int *content_rows){
  char **wrapped = nsent > 0 ? calloc((size_t)nsent, sizeof(char *)) : NULL;
  if(nsent > 0 && wrapped){
    for(int i = 0; i < nsent; i++) wrapped[i] = wrap_sentence(sent[i], text_width);
  }

  int total = 0;
  for(int i = 0; i < nsent; i++){
    if(wrapped && wrapped[i]){
      int lines = 1;
      for(const char *c = wrapped[i]; *c; c++) if(*c == '\n') lines++;
      total += lines + line_gap;
    } else {
      int len = (int)strlen(sent[i]);
      total += (text_width > 0 ? len / text_width : len) + 1 + line_gap;
    }
  }
  int pad_height = total + 2;
  if(pad_height < rows + 1) pad_height = rows + 1;

  WINDOW *pad = newpad(pad_height, cols);
  if(pad == NULL){
    if(wrapped){
      for(int i = 0; i < nsent; i++) free(wrapped[i]);
      free(wrapped);
    }
    return NULL;
  }
  wbkgd(pad, COLOR_PAIR(color_pair));

  int row = 0;
  sent_row[0] = 0;
  for(int i = 0; i < nsent; i++){
    if(wrapped && wrapped[i]){
      const char *ls = wrapped[i];
      for(;;){
        const char *nl = strchr(ls, '\n');
        int len = nl ? (int)(nl - ls) : (int)strlen(ls);
        if(len > 0) mvwaddnstr(pad, row, text_col, ls, len);
        row++;
        if(!nl) break;
        ls = nl + 1;
      }
    } else {
      const char *b = sent[i];
      int rem = (int)strlen(b);
      if(rem == 0) row++;
      while(rem > 0){
        int len = rem > text_width ? text_width : rem;
        if(len < rem)
          while(len > 1 && ((unsigned char)b[len] & 0xC0) == 0x80) len--;
        mvwaddnstr(pad, row, text_col, b, len);
        row++;
        b += len;
        rem -= len;
      }
    }
    row += line_gap;
    sent_row[i+1] = row;
  }

  if(wrapped){
    for(int i = 0; i < nsent; i++) free(wrapped[i]);
    free(wrapped);
  }
  *content_rows = nsent > 0 ? sent_row[nsent] : 0;
  return pad;
}

static int page_to(const int *sent_row, int nsent, int cur, int delta){
  int target = sent_row[cur] + delta;
  if(target < 0) target = 0;

  int best = 0;
  for(int i = 0; i < nsent; i++){
    if(sent_row[i] <= target) best = i;
    else break;
  }

  if(delta > 0 && best <= cur) best = (cur < nsent - 1) ? cur + 1 : cur;
  if(delta < 0 && best >= cur) best = (cur > 0)         ? cur - 1 : cur;
  return best;
}

static void set_layout(int width, int cols){
  int w = (width <= 0 || width > cols) ? cols : width;
  if(w < 1) w = 1;
  text_width = w;
  text_col = (cols - w) / 2;
  line_gap = (w < cols) ? 1 : 0;
}

static void apply_theme(int idx){
  if(idx <= 0 || idx >= n_themes){
    color_pair = 0;
    return;
  }
  const theme_t *t = &themes[idx];
  if(can_change_color()){
    init_color(COLOR_YELLOW, t->fg[0], t->fg[1], t->fg[2]);
    init_color(COLOR_BLUE,   t->bg[0], t->bg[1], t->bg[2]);
    init_pair(1, COLOR_YELLOW, COLOR_BLUE);
  } else {
    init_pair(1, t->fg_basic, t->bg_basic);
  }
  color_pair = 1;
}

static void apply_base(WINDOW *pad, const int *sent_row, int nsent){
  int rows = sent_row[nsent];
  for(int r = 0; r < rows; r++)
    mvwchgat(pad, r, text_col, text_width, normal_attr, color_pair, NULL);
}

static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr){
  int end = row[i+1] - line_gap;
  for(int r = row[i]; r < end; r++)
    mvwchgat(pad, r, text_col, text_width, attr, color_pair, NULL);
}

static void goto_line(WINDOW *pad, const int *row, int content_rows,
                      int rows, int cols, int *top, int old, int cur){
  if(old >= 0 && old != cur) paint_line(pad, row, old, normal_attr);
  paint_line(pad, row, cur, A_REVERSE);

  {
    int target = row[cur] - rows / 2;
    int maxtop = content_rows - rows;
    if(maxtop < 0) maxtop = 0;
    if(target < 0) target = 0;
    if(target > maxtop) target = maxtop;
    *top = target;
  }

  prefresh(pad, *top, 0, 0, 0, rows - 1, cols - 1);
}

static double kara_now(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int kara_token_at(double frac){
  if(knspan <= 0) return -1;
  if(frac < 0.0) frac = 0.0;
  if(frac > 1.0) frac = 1.0;
  double target = frac * (double)ktotal;
  int acc = 0;
  for(int i = 0; i < knspan; i++){
    acc += kspan[i].cells;
    if((double)acc > target) return i;
  }
  return knspan - 1;
}

static void kara_paint_tok(WINDOW *pad, int base_row, int i, attr_t attr){
  if(i < 0 || i >= knspan) return;
  mvwchgat(pad, base_row + kspan[i].row, text_col + kspan[i].col,
           kspan[i].cells, attr, color_pair, NULL);
}

static void kara_build(char **sent, int cur){
  free(kspan);
  kspan = NULL;
  knspan = wrap_words(sent[cur], text_width, &kspan);
  ktotal = 0;
  for(int i = 0; i < knspan; i++) ktotal += kspan[i].cells;
  kidx = -1;
}

static void kara_frame(WINDOW *pad, const int *sent_row, int cur){
  paint_line(pad, sent_row, cur, A_NORMAL);
  int w = kara_token_at(kdur > 0.0 ? (kara_now() - kt0) / kdur : 0.0);
  kidx = w;
  kara_paint_tok(pad, sent_row[cur], w, A_REVERSE);
}

static void kara_begin(WINDOW *pad, char **sent, const int *sent_row, int cur,
                       const char *pfile, int rate, int top, int view_rows,
                       int cols){
  kara_build(sent, cur);
  kdur = audio_duration(pfile);
  if(kdur <= 0.0)
    kdur = (double)(knspan > 0 ? knspan : 1) * 60.0 /
           (double)(rate > 0 ? rate : 180);
  kt0 = kara_now();
  kplaying = 1;
  if(kara_on && knspan > 0){
    kara_frame(pad, sent_row, cur);
    prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
  }
}

static void kara_advance(WINDOW *pad, const int *sent_row, int cur,
                         int top, int view_rows, int cols){
  if(!kara_on || !kplaying || knspan <= 0) return;
  double frac = kdur > 0.0 ? (kara_now() - kt0) / kdur : 1.0;
  int w = kara_token_at(frac);
  if(w != kidx){
    kara_paint_tok(pad, sent_row[cur], kidx, A_NORMAL);
    kara_paint_tok(pad, sent_row[cur], w, A_REVERSE);
    kidx = w;
    prefresh(pad, top, 0, 0, 0, view_rows - 1, cols - 1);
  }
}

static void kara_stop(void){
  kplaying = 0;
  free(kspan);
  kspan = NULL;
  knspan = 0;
  ktotal = 0;
  kidx = -1;
}

static void draw_status(WINDOW *sbar, const char *name, int cur, int nsent,
                        int playing, int rate, int words, int cols){
  werase(sbar);
  wmove(sbar, 0, 1);
  if(nsent > 0){
    int pct = ((cur + 1) * 100) / nsent;
    wprintw(sbar, "%s   %d/%d  %d%%   %d words   %s   %d wpm",
            name, cur + 1, nsent, pct, words,
            playing ? "playing" : "paused", rate);
  } else {
    wprintw(sbar, "%s   (no readable text)", name);
  }

  const char *hint = "Space play/pause   Up/Dn move   / find   o outline   +/- speed   , settings   ? help   f focus   w word   t theme   [ ] width   q quit ";
  int hlen = (int)strlen(hint);
  if(cols - hlen > getcurx(sbar) + 2)
    mvwprintw(sbar, 0, cols - hlen, "%s", hint);

  wrefresh(sbar);
}

static void stop_audio(pid_t *synth_pid, pid_t *play_pid,
                       int *synth_i, int *ready){
  if(*play_pid > 0){
    kill(*play_pid, SIGTERM);
    waitpid(*play_pid, NULL, 0);
    *play_pid = 0;
  }
  if(*synth_pid > 0){
    kill(*synth_pid, SIGTERM);
    waitpid(*synth_pid, NULL, 0);
    *synth_pid = 0;
  }
  *synth_i = -1;
  *ready = -1;
}
