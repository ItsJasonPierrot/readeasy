#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "input.h"
#include "ui.h"

#ifndef READEASY_VERSION
#define READEASY_VERSION "1.0.0"
#endif

static void usage(FILE *f){
  fputs(
"Usage: readeasy [options] [file]\n"
"\n"
"Read a text file aloud in the terminal. With no file, reads piped input.\n"
"\n"
"Options:\n"
"  -r, --rate N     starting speed in words per minute (80-400, default 180)\n"
"  -w, --width N    wrap text to N columns and centre it (min 20)\n"
"      --voice NAME text-to-speech voice (passed to `say -v`)\n"
"      --focus      dim everything except the current sentence\n"
"      --theme NAME color theme: none, blue, cream, contrast, dark (default none)\n"
"      --color      shorthand for --theme blue\n"
"  -v, --version    print version and exit\n"
"  -h, --help       print this help and exit\n"
"\n"
"Controls are shown in the status bar while a file is open.\n", f);
}

int main(int argc, char *argv[]){
  char *buffer;
  int input_text = STDIN_FILENO;
  ui_opts opts = { .rate = RATE_DEFAULT, .voice = NULL, .theme = 0,
                   .focus = 0, .width = 0 };

  static struct option longopts[] = {
    {"rate",     required_argument, 0, 'r'},
    {"width",    required_argument, 0, 'w'},
    {"theme",    required_argument, 0, 'T'},
    {"voice",    required_argument, 0, 'V'},
    {"focus",    no_argument,       0, 'F'},
    {"color",    no_argument,       0, 'C'},
    {"no-color", no_argument,       0, 'N'},
    {"version",  no_argument,       0, 'v'},
    {"help",     no_argument,       0, 'h'},
    {0, 0, 0, 0}
  };

  int c;
  while((c = getopt_long(argc, argv, "r:w:vh", longopts, NULL)) != -1){
    switch(c){
      case 'r': {
        char *end;
        long v = strtol(optarg, &end, 10);
        if(*optarg == '\0' || *end != '\0' || v < RATE_MIN || v > RATE_MAX){
          fprintf(stderr,
                  "readeasy: --rate must be a number between %d and %d\n",
                  RATE_MIN, RATE_MAX);
          return 1;
        }
        opts.rate = (int)v;
        break;
      }
      case 'w': {
        char *end;
        long v = strtol(optarg, &end, 10);
        if(*optarg == '\0' || *end != '\0' || v < WIDTH_MIN){
          fprintf(stderr,
                  "readeasy: --width must be a number of at least %d\n",
                  WIDTH_MIN);
          return 1;
        }
        opts.width = (int)v;
        break;
      }
      case 'T': {
        int idx = ui_theme_index(optarg);
        if(idx < 0){
          fprintf(stderr, "readeasy: unknown theme '%s'. Options:", optarg);
          for(int i = 0; i < ui_theme_count(); i++)
            fprintf(stderr, " %s", ui_theme_name(i));
          fprintf(stderr, "\n");
          return 1;
        }
        opts.theme = idx;
        break;
      }
      case 'V': opts.voice = optarg;              break;
      case 'F': opts.focus = 1;                   break;
      case 'C': opts.theme = ui_theme_index("blue"); break;
      case 'N': opts.theme = 0;                   break;
      case 'v': printf("readeasy %s\n", READEASY_VERSION); return 0;
      case 'h': usage(stdout);            return 0;
      default:  usage(stderr);            return 1;
    }
  }

  int nargs = argc - optind;
  if(nargs > 1){
    usage(stderr);
    return 1;
  }
  const char *file = (nargs == 1) ? argv[optind] : NULL;

  if(file == NULL && isatty(STDIN_FILENO)){
    write(STDERR_FILENO, "No input provided.\n", 19);
    return 1;
  }

  if(file != NULL){
    input_text = open(file, O_RDONLY);
    if(input_text < 0){
      write(STDERR_FILENO, "File not found.\n", 16);
      return 1;
    }
  }

  if(process_buffer(input_text, &buffer) != 0){
    if(file != NULL) close(input_text);
    return 1;
  }

  if(file != NULL) close(input_text);

  if(!isatty(STDIN_FILENO)){
    if(freopen("/dev/tty", "r", stdin) == NULL){
      write(STDERR_FILENO, "No terminal available for controls.\n", 36);
      free(buffer);
      return 1;
    }
  }

  const char *name = (file != NULL) ? file : "(stdin)";

  if(run_ui(buffer, name, &opts) != 0){
    free(buffer);
    return 1;
  }

  free(buffer);

  return 0;
}
