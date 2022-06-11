#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // readlink
#include <string.h> // strcpy, strerror
#include <linux/limits.h> // PATH_MAX
#include <fcntl.h> // S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH
#include <stdarg.h> // va_list, va_start, va_arg, va_end
#include <errno.h> // errno
#include <sys/stat.h> // mkdir
#include "defines.h"

int log_fd;
void server_log_init();
void connection_log_init(int);
void log_close();
void log_print(char*, ...);
extern int close_descriptor(int d);

#endif // LOGGER_H
