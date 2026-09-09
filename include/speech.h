#ifndef SPEECH_H
#define SPEECH_H
#include <sys/types.h>

int synth_to_file(const char *text, const char *path, int rate, pid_t *pid);

int play_file(const char *path, pid_t *pid);

#endif
