#ifndef SHOWBOARD_H
#define SHOWBOARD_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h> // open, O_RDONLY

//#include <assert.h>
#include <dirent.h> // DIR, struct dirent, PATH_MAX
#include <sys/stat.h>
//#include <sys/types.h>
#include "defines.h"

extern char* showboard_dir;
extern ssize_t showboard_dir_length;

int showboard(int conn);

#endif // SHOWBOARD_H
