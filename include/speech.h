#ifndef SPEECH_H
#define SPEECH_H
#include <sys/types.h>

/* Synthesize `text` to the audio file at `path` (via `say -o`) without
 * playing it, so the next sentence can be prepared while the current one
 * plays. Starts the process and returns its pid in *pid. */
int synth_to_file(const char *text, const char *path, pid_t *pid);

/* Play an already-synthesized audio file (via `afplay`). */
int play_file(const char *path, pid_t *pid);

#endif
