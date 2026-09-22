#ifndef PLACES_H
#define PLACES_H

typedef struct {
  int sent;
  char *name;
} bookmark;

int places_load(const char *path, int *pos, bookmark **marks, int *nmarks);
int places_save(const char *path, int pos, const bookmark *marks, int nmarks);
void places_free(bookmark *marks, int nmarks);

#endif
