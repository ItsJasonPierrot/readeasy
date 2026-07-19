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

/* Playback runs a tiny pipeline so audio doesn't stutter between sentences:
 * while one sentence plays, the next is already being synthesized to a file.
 *   STOPPED  - nothing playing
 *   SYNTH    - synthesizing the current sentence; play it once ready
 *   PLAY     - current sentence playing; next is prefetching in the background
 */
enum { STOPPED, SYNTH, PLAY };

static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr);
static void goto_line(WINDOW *pad, const int *row, int content_rows,
                      int rows, int cols, int *top, int old, int cur);
static void stop_audio(pid_t *synth_pid, pid_t *play_pid,
                       int *synth_i, int *ready);

int run_ui(char *text){
  int rows, cols;
  int pad_height, content_rows;
  int top = 0;
  int cur = 0;             /* current sentence: highlighted, and where speech begins */
  int nsent = 0;
  int character;
  int *sent_row = NULL;    /* pad row each sentence starts on */
  char **sent;
  WINDOW *pad;

  /* Audio pipeline state. */
  int astate = STOPPED;
  pid_t synth_pid = 0, play_pid = 0;
  int synth_i = -1;        /* sentence being synthesized, or -1 */
  int ready = -1;          /* sentence whose prefetched audio is ready in pnext, or -1 */
  char patha[64], pathb[64];
  char *pcur = patha, *pnext = pathb, *pswap;
  char tmpl[] = "/tmp/readeasy.XXXXXX";
  char *tmpdir;
  int audio_ok;

  sent = build_sentences(text, &nsent);

  initscr();
  setlocale(LC_ALL, "");
  start_color();
  if (can_change_color()) {
    init_color(COLOR_BLUE,   60, 100, 250);
    init_color(COLOR_YELLOW, 1000, 780, 560);
  }
  init_pair(1, COLOR_YELLOW, COLOR_BLUE);   /* keep this line as-is */
  noecho();
  cbreak();
  keypad(stdscr, TRUE);

  /* Recognize the arrow keys in both "normal" (ESC [ A) and "application"
   * (ESC O A) cursor-key encodings. Some terminals (e.g. Ghostty) reset
   * the cursor-key mode when you switch away to another tab and back; if
   * that happens the arrows arrive in the other encoding, and without
   * these fallbacks ncurses would stop recognizing them. */
  define_key("\033[A", KEY_UP);
  define_key("\033[B", KEY_DOWN);
  define_key("\033OA", KEY_UP);
  define_key("\033OB", KEY_DOWN);

  getmaxyx(stdscr, rows, cols);

  /* Color the whole physical screen, not just the pad, so short text still
   * fills the window instead of leaving the terminal's default background. */
  wbkgd(stdscr, COLOR_PAIR(1));
  clear();
  refresh();

  /* Two temp files hold the current and prefetched sentence audio. */
  tmpdir = mkdtemp(tmpl);
  audio_ok = (tmpdir != NULL) && (sent != NULL);
  if(audio_ok){
    snprintf(patha, sizeof patha, "%s/a.aiff", tmpdir);
    snprintf(pathb, sizeof pathb, "%s/b.aiff", tmpdir);
  }

  /* Lay the sentences out re-flowed to the window width: each sentence is
   * written as its own block and ncurses wraps it at `cols`, so long
   * sentences fill the width instead of the file's original narrow wrap. */
  {
    int total = 0;
    for(int i = 0; i < nsent; i++) total += (int)strlen(sent[i]);
    pad_height = total / cols + 2 * nsent + 4;
    if(pad_height < rows + 1) pad_height = rows + 1;
  }

  pad = newpad(pad_height, cols);
  wbkgd(pad, COLOR_PAIR(1));

  sent_row = malloc((size_t)(nsent + 1) * sizeof(int));
  if(sent_row == NULL){
    endwin();
    free_sentences(sent, nsent);
    return 1;
  }
  sent_row[0] = 0;
  {
    int y, x;
    for(int i = 0; i < nsent; i++){
      waddstr(pad, sent[i]);
      waddch(pad, '\n');
      getyx(pad, y, x);
      (void)x;
      sent_row[i+1] = y;
    }
  }
  content_rows = nsent > 0 ? sent_row[nsent] : 0;

  if(nsent > 0)
    goto_line(pad, sent_row, content_rows, rows, cols, &top, -1, cur);
  else
    prefresh(pad, 0, 0, 0, 0, rows - 1, cols - 1);

  while(1){
    timeout(astate == STOPPED ? -1 : 100);
    character = getch();

    if(character == KEY_RESIZE || character == 12 /* Ctrl-L */){
      getmaxyx(stdscr, rows, cols);
      flushinp();
      wbkgd(stdscr, COLOR_PAIR(1));
      clearok(curscr, TRUE);
      touchwin(stdscr);
      refresh();
      if(nsent > 0)
        goto_line(pad, sent_row, content_rows, rows, cols, &top, cur, cur);
      else
        prefresh(pad, 0, 0, 0, 0, rows - 1, cols - 1);
      continue;
    }

    if((character == KEY_DOWN || character == KEY_UP) && nsent > 0){
      if(astate != STOPPED){          /* moving the cursor pauses playback */
        stop_audio(&synth_pid, &play_pid, &synth_i, &ready);
        astate = STOPPED;
      }
      int old = cur;
      if(character == KEY_DOWN && cur < nsent - 1) cur++;
      if(character == KEY_UP   && cur > 0)         cur--;
      goto_line(pad, sent_row, content_rows, rows, cols, &top, old, cur);
    }

    if(character == ' ' && nsent > 0 && audio_ok){
      if(astate == STOPPED){
        /* Start: synthesize the current sentence, then play it. */
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

    /* Drive the audio pipeline. */
    if(astate == SYNTH){
      if(waitpid(synth_pid, NULL, WNOHANG) > 0){
        synth_i = -1;
        if(play_file(pcur, &play_pid) == 0){
          astate = PLAY;
          if(cur + 1 < nsent &&                       /* prefetch the next one */
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
        ready = synth_i;            /* prefetched audio is now on disk */
        synth_i = -1;
      }
      if(waitpid(play_pid, NULL, WNOHANG) > 0){
        if(cur + 1 >= nsent){
          astate = STOPPED;
        } else {
          int old = cur;
          cur++;
          goto_line(pad, sent_row, content_rows, rows, cols, &top, old, cur);
          pswap = pcur; pcur = pnext; pnext = pswap;   /* cur's audio is in pnext */
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
            astate = SYNTH;          /* still synthesizing cur; play when ready */
          } else if(synth_to_file(sent[cur], pcur, &synth_pid) == 0){
            synth_i = cur;
            astate = SYNTH;
          } else {
            astate = STOPPED;
          }
        }
      }
    }
  }

  delwin(pad);
  endwin();
  free(sent_row);
  free_sentences(sent, nsent);
  if(audio_ok){
    unlink(patha);
    unlink(pathb);
    rmdir(tmpdir);
  }
  return 0;
}

/* Set the color/attribute of every pad row that sentence i occupies. */
static void paint_line(WINDOW *pad, const int *row, int i, attr_t attr){
  for(int r = row[i]; r < row[i+1]; r++)
    mvwchgat(pad, r, 0, -1, attr, 1, NULL);
}

/* Move the highlight from `old` to `cur`, scroll so `cur` is visible, and
 * repaint. Pass old == -1 to only paint the new line. */
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

/* Kill and reap any running synth/play processes. */
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
