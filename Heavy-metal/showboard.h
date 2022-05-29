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
#include <poll.h>
#include "defines.h"

#ifndef NDEBUG
  #include "logger.h"
#endif

extern char* showboard_dir;
extern ssize_t showboard_dir_length;
extern ssize_t write_block(const int conn, const char* buf, const ssize_t count);

int showboard(int conn);

#endif // SHOWBOARD_H
