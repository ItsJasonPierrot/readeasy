#include <ncurses.h>
#include <signal.h>
#include <sys/wait.h>
#include <locale.h>
#include "speech.h"
#include "ui.h"

static const char *line_start(const char *src, int n, int cols);
static int total_rows(const char *src, int cols);

int run_ui(char *text){
  pid_t pid;
  int speaking = 0;
  int rows, cols;
  int pad_height;
  int top = 0;
  int character;
  WINDOW *pad;

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

  getmaxyx(stdscr, rows, cols);

  /* Color the whole physical screen, not just the pad: if the text is
   * shorter than the screen, the pad doesn't cover every row, and
   * without this the area below it stays the terminal's default
   * background instead of matching. */
  wbkgd(stdscr, COLOR_PAIR(1));
  clear();
  refresh();

  pad_height = total_rows(text, cols) + 1;

  pad = newpad(pad_height, cols);
  wbkgd(pad, COLOR_PAIR(1));
  waddstr(pad, text);

  prefresh(pad, top, 0, 0, 0, rows - 1, cols - 1);

  while(1){
    character = getch();

    if(character == KEY_DOWN){
      if(top < pad_height - rows) top++;
      prefresh(pad, top, 0, 0, 0, rows - 1, cols - 1);
    }

    if(character == KEY_UP){
      if(top > 0) top--;
      prefresh(pad, top, 0, 0, 0, rows - 1, cols - 1);
    }

    if(character == ' '){
      if(!speaking){
        if(speak(line_start(text, top, cols), &pid) != 0){
          endwin();
          return 1;
        }
        speaking = 1;
      } else {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        speaking = 0;
      }
    }

    if(character == 'q'){
      if(speaking){
        kill(pid, SIGTERM);   // stop say
        waitpid(pid, NULL, 0);// reap it (blocking ok here — we're quitting)
      }
      break;
    }

    if(speaking){
      if(waitpid(pid, NULL, WNOHANG) > 0){
        speaking = 0;
      }
    }
  }  
  
  delwin(pad);
  endwin();
  return 0;
}

static const char *line_start(const char *src, int n, int cols){
    /* Walk the text the same way ncurses lays it out in the pad: a row
     * ends at an explicit '\n' or after `cols` screen columns. Each
     * UTF-8 codepoint (lead byte plus any continuation bytes) is
     * consumed as a single column, and always as a whole unit, so the
     * returned pointer never lands mid-character and column counts
     * don't drift on multi-byte text. */
    int row = 0;
    int col = 0;

    while(*src && row < n){
        if(*src == '\n'){
            row++;
            col = 0;
            src++;
            continue;
        }

        src++;
        while(((unsigned char)*src & 0xC0) == 0x80) src++;
        col++;

        if(col == cols){
            row++;
            col = 0;
        }
    }
    return src;   /* points at the first character of pad row n */
}

static int total_rows(const char *src, int cols){
    /* Same wrapping rules as line_start(), counting every row the pad
     * will actually use so the scroll bound (and background fill)
     * match what's really on screen. */
    int row = 1;
    int col = 0;

    while(*src){
        if(*src == '\n'){
            row++;
            col = 0;
            src++;
            continue;
        }

        src++;
        while(((unsigned char)*src & 0xC0) == 0x80) src++;
        col++;

        if(col == cols){
            row++;
            col = 0;
        }
    }
    return row;
}
