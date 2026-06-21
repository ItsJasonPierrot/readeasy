#include <ncurses.h>
#include <signal.h>
#include <sys/wait.h>
#include "speech.h"
#include "ui.h"

int run_ui(char *text){
  pid_t pid;
  int speaking = 0;
  initscr();
  clear();

  noecho();
  cbreak();

  printw("%s", text);
  refresh();
  
  while(1){
    int character = getch();

    if(character == ' '){
      if(!speaking){
        if(speak(text,&pid) != 0){
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
      if(waitpid(pid,NULL,WNOHANG) > 0){
        speaking = 0;
      }
    }
  }  
  
  endwin();
  return 0;
}
