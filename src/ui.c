#include <ncurses.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include "speech.h"
#include "ui.h"

int run_ui(char *text){
  pid_t pid;
  int speaking = 0;
  int rows, cols;
  int newlines = 0;
  int pad_height;
  int top = 0;
  int character;
  char *p;
  WINDOW *pad;

  initscr();
  noecho();
  cbreak();
  keypad(stdscr, TRUE);

  getmaxyx(stdscr, rows, cols);

  for (p = text; *p; p++)
    if(*p == '\n') newlines++;
  pad_height = (strlen(text) / cols) + newlines + 2;

  pad = newpad(pad_height, cols);
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
        if(speak(text, &pid) != 0){
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
