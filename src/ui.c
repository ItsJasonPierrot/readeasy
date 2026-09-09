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

static volatile sig_atomic_t curses_active = 0;
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
static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr);
static void goto_line(WINDOW *pad, const int *row, int content_rows,
                      int rows, int cols, int *top, int old, int cur);
static void stop_audio(pid_t *synth_pid, pid_t *play_pid,
                       int *synth_i, int *ready);
static void draw_status(WINDOW *sbar, const char *name, int cur, int nsent,
                        int playing, int cols);

int run_ui(char *text, const char *name){
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
  char *pcur = audio_a, *pnext = audio_b, *pswap;

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
  if (can_change_color()) {
    init_color(COLOR_BLUE,   60, 100, 250);
    init_color(COLOR_YELLOW, 1000, 780, 560);
  }
  init_pair(1, COLOR_YELLOW, COLOR_BLUE);
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  curs_set(0);

  define_key("\033[A", KEY_UP);
  define_key("\033[B", KEY_DOWN);
  define_key("\033OA", KEY_UP);
  define_key("\033OB", KEY_DOWN);

  getmaxyx(stdscr, rows, cols);
  has_status = rows > 1;
  view_rows = has_status ? rows - 1 : rows;

  wbkgd(stdscr, COLOR_PAIR(1));
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
    if(sbar) wbkgd(sbar, COLOR_PAIR(1) | A_REVERSE);
  }

  if(nsent > 0)
    goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
  else
    prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
  if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, cols);

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
        if(sbar) wbkgd(sbar, COLOR_PAIR(1) | A_REVERSE);
      }
      flushinp();
      wbkgd(stdscr, COLOR_PAIR(1));
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
      if(nsent > 0)
        goto_line(pad, sent_row, content_rows, view_rows, cols, &top, -1, cur);
      else
        prefresh(pad, 0, 0, 0, 0, view_rows - 1, cols - 1);
      if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, cols);
      continue;
    }

    if((character == KEY_DOWN || character == KEY_UP) && nsent > 0){
      if(astate != STOPPED){
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        astate = STOPPED;
      }
      int old = cur;
      if(character == KEY_DOWN && cur < nsent - 1) cur++;
      if(character == KEY_UP   && cur > 0)         cur--;
      goto_line(pad, sent_row, content_rows, view_rows, cols, &top, old, cur);
    }

    if(character == ' ' && nsent > 0 && audio_ok){
      if(astate == STOPPED){
        if(synth_to_file(sent[cur], pcur, &synth_pid) == 0){
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

    if(astate == SYNTH){
      if(waitpid(synth_pid, NULL, WNOHANG) > 0){
        synth_i = -1;
        if(play_file(pcur, &play_pid) == 0){
          astate = PLAY;
          if(cur + 1 < nsent &&
             synth_to_file(sent[cur+1], pnext, &synth_pid) == 0){
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
                 synth_to_file(sent[cur+1], pnext, &synth_pid) == 0){
                synth_i = cur + 1;
              }
            } else {
              astate = STOPPED;
            }
          } else if(synth_i == cur){
            astate = SYNTH;
          } else if(synth_to_file(sent[cur], pcur, &synth_pid) == 0){
            synth_i = cur;
            astate = SYNTH;
          } else {
            astate = STOPPED;
          }
        }
      }
    }
    if(sbar) draw_status(sbar, name, cur, nsent, astate != STOPPED, cols);
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
  int total = 0;
  for(int i = 0; i < nsent; i++) total += (int)strlen(sent[i]);
  int pad_height = total / cols + 2 * nsent + 4;
  if(pad_height < rows + 1) pad_height = rows + 1;

  WINDOW *pad = newpad(pad_height, cols);
  if(pad == NULL) return NULL;
  wbkgd(pad, COLOR_PAIR(1));

  sent_row[0] = 0;
  int y, x;
  for(int i = 0; i < nsent; i++){
    waddstr(pad, sent[i]);
    waddch(pad, '\n');
    getyx(pad, y, x);
    (void)x;
    sent_row[i+1] = y;
  }
  *content_rows = nsent > 0 ? sent_row[nsent] : 0;
  return pad;
}

static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr){
  for(int r = row[i]; r < row[i+1]; r++)
    mvwchgat(pad, r, 0, -1, attr, 1, NULL);
}

static void goto_line(WINDOW *pad, const int *row, int content_rows,
                      int rows, int cols, int *top, int old, int cur){
  if(old >= 0 && old != cur) paint_line(pad, row, old, A_NORMAL);
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
                        int playing, int cols){
  werase(sbar);
  wmove(sbar, 0, 1);
  if(nsent > 0){
    int pct = ((cur + 1) * 100) / nsent;
    wprintw(sbar, "%s   %d/%d  %d%%   %s",
            name, cur + 1, nsent, pct, playing ? "playing" : "paused");
  } else {
    wprintw(sbar, "%s   (no readable text)", name);
  }

  const char *hint = "Space play/pause   Up/Dn move   q quit ";
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
