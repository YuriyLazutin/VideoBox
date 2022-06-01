#ifndef PUMP_H
#define PUMP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // strlen

#include <fcntl.h> // O_RDONLY
#include <sys/sendfile.h> // sendfile
#include <sys/stat.h> // struct stat
#include <linux/limits.h> // PATH_MAX
//#include <dirent.h> // DIR, struct dirent, PATH_MAX
//#include <sys/types.h>

#include "defines.h"

#ifndef NDEBUG
  #include "logger.h"
#endif

extern char* showboard_dir;
extern struct block_range *blocks;
extern ssize_t write_block(const int conn, const char* buf, const ssize_t count);

int pump(int conn, const char* params);

#endif // PUMP_H
