#ifndef SPEECH_H
#define SPEECH_H
#include <sys/types.h>

#ifdef __APPLE__
#define AUDIO_EXT "aiff"
#else
#define AUDIO_EXT "wav"
#endif

int synth_to_file(const char *text, const char *path, int rate,
                  const char *voice, pid_t *pid);

int play_file(const char *path, pid_t *pid);

double audio_duration(const char *path);

char **list_voices(int *n);
void free_voices(char **voices, int n);

#endif
