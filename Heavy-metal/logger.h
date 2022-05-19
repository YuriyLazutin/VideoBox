#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // readlink
#include <string.h> // strcpy
#include <linux/limits.h> // PATH_MAX
#include <fcntl.h> // S_IRUSR, S_IWUSR, S_IRGRP, S_IWGRP, S_IROTH
#include <stdarg.h> // va_list, va_start, va_arg, va_end
#include "defines.h"

int connection_id;

int server_log_init();
int connection_log_init();
void log_close(int);

void log_print(int, char*, ...);

#endif // LOGGER_H
