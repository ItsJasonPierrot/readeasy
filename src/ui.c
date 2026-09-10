#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <locale.h>
#include "speech.h"
#include "reflow.h"
#include "ui.h"

enum { STOPPED, SYNTH, PLAY };

#define RATE_STEP 20

static volatile sig_atomic_t curses_active = 0;
static short color_pair = 1;
static attr_t normal_attr = A_NORMAL;
static pid_t synth_pid = 0;
static pid_t play_pid = 0;
static char audio_dir[] = "/tmp/readeasy.XXXXXX";
static char audio_a[64];
static char audio_b[64];
static int audio_ok = 0;

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
static void apply_base(WINDOW *pad, const int *sent_row, int nsent);
static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr);
static void goto_line(WINDOW *pad, const int *row, int content_rows,
                      int rows, int cols, int *top, int old, int cur);
static void stop_audio(pid_t *synth_pid, pid_t *play_pid,
                       int *synth_i, int *ready);
static void draw_status(WINDOW *sbar, const char *name, int cur, int nsent,
                        int playing, int rate, int cols);

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

  int astate = STOPPED;
  int synth_i = -1;
  int ready = -1;
  int rate = opts->rate;
  int focus = opts->focus;
  const char *voice = opts->voice;
  char *pcur = audio_a, *pnext = audio_b, *pswap;

  if(rate < RATE_MIN) rate = RATE_MIN;
  if(rate > RATE_MAX) rate = RATE_MAX;
  color_pair = opts->color ? 1 : 0;
  normal_attr = focus ? A_DIM : A_NORMAL;

  sent = build_sentences(text, &nsent);

  initscr();
  curses_active = 1;
  signal(SIGINT,  on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGHUP,  on_signal);
  signal(SIGQUIT, on_signal);
  atexit(cleanup);
  setlocale(LC_ALL, "");
  start_color();
  if(opts->color){
    if (can_change_color()) {
      init_color(COLOR_BLUE,   60, 100, 250);
      init_color(COLOR_YELLOW, 1000, 780, 560);
    }
    init_pair(1, COLOR_YELLOW, COLOR_BLUE);
  }
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

  wbkgd(stdscr, COLOR_PAIR(color_pair));
  clear();
  refresh();

  audio_ok = (mkdtemp(audio_dir) != NULL) && (sent != NULL);
  if(audio_ok){
    snprintf(audio_a, sizeof audio_a, "%s/a.aiff", audio_dir);
    snprintf(audio_b, sizeof audio_b, "%s/b.aiff", audio_dir);
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
  if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, rate, cols);

  while(1){
    timeout(astate == STOPPED ? -1 : 100);
    character = getch();

    if(character == KEY_RESIZE || character == 12){
      getmaxyx(stdscr, rows, cols);
      has_status = rows > 1;
      view_rows = has_status ? rows - 1 : rows;
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
      if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, rate, cols);
      continue;
    }

    if(nsent > 0 && (character == KEY_DOWN  || character == KEY_UP    ||
                     character == KEY_NPAGE || character == KEY_PPAGE ||
                     character == KEY_HOME  || character == KEY_END   ||
                     character == 'g'       || character == 'G')){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
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
        astate = STOPPED;
      }
    }

    if(character == 'q'){
      stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
      break;
    }

    if(character == '+' || character == '='){
      rate += RATE_STEP;
      if(rate > RATE_MAX) rate = RATE_MAX;
    }
    if(character == '-' || character == '_'){
      rate -= RATE_STEP;
      if(rate < RATE_MIN) rate = RATE_MIN;
    }

    if(character == 'f'){
      focus = !focus;
      normal_attr = focus ? A_DIM : A_NORMAL;
      if(nsent > 0){
        apply_base(pad, sent_row, nsent);
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
      }
    }

    if(astate == SYNTH){
      if(waitpid(synth_pid, NULL, WNOHANG) > 0){
        synth_i = -1;
        if(play_file(pcur, &play_pid) == 0){
          astate = PLAY;
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
          astate = STOPPED;
        } else {
          int old = cur;
          cur++;
          goto_line(pad, sent_row, content_rows, view_rows, cols, &top, old, cur);
          pswap = pcur; pcur = pnext; pnext = pswap;
          if(ready == cur){
            ready = -1;
            if(play_file(pcur, &play_pid) == 0){
              astate = PLAY;
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
    if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, rate, cols);
  }

  delwin(pad);
  if(sbar) delwin(sbar);
  free(sent_row);
  free_sentences(sent, nsent);
  cleanup();
  return 0;
}

static WINDOW *build_pad(char **sent, int nsent, int rows, int cols,
                         int *sent_row, int *content_rows){
  char **wrapped = nsent > 0 ? calloc((size_t)nsent, sizeof(char *)) : NULL;
  if(nsent > 0 && wrapped){
    for(int i = 0; i < nsent; i++) wrapped[i] = wrap_sentence(sent[i], cols);
  }

  int total = 0;
  for(int i = 0; i < nsent; i++){
    if(wrapped && wrapped[i]){
      int lines = 1;
      for(const char *c = wrapped[i]; *c; c++) if(*c == '\n') lines++;
      total += lines;
    } else {
      int len = (int)strlen(sent[i]);
      total += (cols > 0 ? len / cols : len) + 1;
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
        if(len > 0) mvwaddnstr(pad, row, 0, ls, len);
        row++;
        if(!nl) break;
        ls = nl + 1;
      }
    } else {
      const char *b = sent[i];
      int rem = (int)strlen(b);
      if(rem == 0) row++;
      while(rem > 0){
        int len = rem > cols ? cols : rem;
        mvwaddnstr(pad, row, 0, b, len);
        row++;
        b += len;
        rem -= len;
      }
    }
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

static void apply_base(WINDOW *pad, const int *sent_row, int nsent){
  int rows = sent_row[nsent];
  for(int r = 0; r < rows; r++)
    mvwchgat(pad, r, 0, -1, normal_attr, color_pair, NULL);
}

static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr){
  for(int r = row[i]; r < row[i+1]; r++)
    mvwchgat(pad, r, 0, -1, attr, color_pair, NULL);
}

static void goto_line(WINDOW *pad, const int *row, int content_rows,
                      int rows, int cols, int *top, int old, int cur){
  if(old >= 0 && old != cur) paint_line(pad, row, old, normal_attr);
  paint_line(pad, row, cur, A_REVERSE);

  if(row[cur] < *top)                 *top = row[cur];
  else if(row[cur] > *top + rows - 1)  *top = row[cur] - rows + 1;

  if(*top < 0) *top = 0;
  {
    int maxtop = content_rows - rows;
    if(maxtop < 0) maxtop = 0;
    if(*top > maxtop) *top = maxtop;
  }

  prefresh(pad, *top, 0, 0, 0, rows - 1, cols - 1);
}

static void draw_status(WINDOW *sbar, const char *name, int cur, int nsent,
                        int playing, int rate, int cols){
  werase(sbar);
  wmove(sbar, 0, 1);
  if(nsent > 0){
    int pct = ((cur + 1) * 100) / nsent;
    wprintw(sbar, "%s   %d/%d  %d%%   %s   %d wpm",
            name, cur + 1, nsent, pct, playing ? "playing" : "paused", rate);
  } else {
    wprintw(sbar, "%s   (no readable text)", name);
  }

  const char *hint = "Space play/pause   Up/Dn move   +/- speed   f focus   q quit ";
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
