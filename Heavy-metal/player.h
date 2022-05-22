#ifndef PLAYER_H
#define PLAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // strlen
#include <linux/limits.h> // PATH_MAX
#include "defines.h"

#ifndef NDEBUG
  #include "logger.h"
#endif

extern char* showboard_dir;
extern ssize_t showboard_dir_length;

int player_show(int conn, const char* params);

#endif // PLAYER_H
