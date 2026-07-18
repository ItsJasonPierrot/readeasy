#ifndef SPEECH_H        
#define SPEECH_H
#include <sys/types.h>    

int speak(const char *text, pid_t *pid);

#endif
