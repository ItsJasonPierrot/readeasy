#ifndef CONFIG_H
#define CONFIG_H

#include "ui.h"

void config_load(ui_opts *opts);
int config_save(const ui_opts *opts);

#endif
