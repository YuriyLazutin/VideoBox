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

#include "defines.h"
#include "common.h"

extern char* showboard_dir;
extern ssize_t showboard_dir_length;
extern struct block_range *blocks;
extern ssize_t write_block(const int conn, const char* buf, const ssize_t count);

int pump(int conn, const char* params);

#endif // PUMP_H
