#ifndef PUMP_H
#define PUMP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // strlen

#include <fcntl.h> // O_RDONLY
#include <sys/sendfile.h> // sendfile
#include <sys/stat.h> // struct stat
#include <dirent.h> // DIR, struct dirent, PATH_MAX
//#include <sys/types.h>

#include "defines.h"

extern char* showboard_dir;

int pump(int conn, const char* params);

#endif // PUMP_H
