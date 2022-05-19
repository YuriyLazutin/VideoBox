#include "logger.h"

int server_log_init()
{
  connection_id = 0;
  char* server_log_path = malloc(PATH_MAX);
  if (!server_log_path)
    return STDERR_FILENO;
  ssize_t length = readlink("/proc/self/exe", server_log_path, PATH_MAX - 1);
  if (length == -1)
    return STDERR_FILENO;
  while (length > 0 && server_log_path[length - 1] != '/')
    length--;
  if (!length)
    return STDERR_FILENO;
  if (length + strlen("log/server.log") + 1 > PATH_MAX)
    return STDERR_FILENO;
  strcpy(server_log_path + length, "log/server.log");
  length += strlen("log/server.log");

  mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;

  int fd = open(server_log_path, O_WRONLY | O_CREAT, mode);
  if (fd == -1)
    return STDERR_FILENO;

  return fd;
}

int connection_log_init()
{
  connection_id++;
  char* connection_log_path = malloc(PATH_MAX);
  if (!connection_log_path)
    return STDERR_FILENO;
  ssize_t length = readlink("/proc/self/exe", connection_log_path, PATH_MAX - 1);
  if (length == -1)
    return STDERR_FILENO;
  while (length > 0 && connection_log_path[length - 1] != '/')
    length--;
  if (!length)
    return STDERR_FILENO;

  char conn_file_log[SMALL_BUFFER_SIZE];
  int len2 = snprintf(conn_file_log, SMALL_BUFFER_SIZE, "log/%8d.log", connection_id);
  if (len2 >= SMALL_BUFFER_SIZE || length + len2 + 1 > PATH_MAX)
    return STDERR_FILENO;
  strcpy(connection_log_path + length, conn_file_log);
  length += len2;

  mode_t mode = S_IRUSR | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH;

  int fd = open(connection_log_path, O_WRONLY | O_CREAT, mode);
  if (fd == -1)
    return STDERR_FILENO;

  return fd;
}

void log_close(int fd)
{
  close(fd);
}

void log_print(int fd, char* format, ...)
{
  FILE* fp = NULL;
  fp = fdopen(fd, "w");
  if (fp == NULL) // Bad convertion
    return;

  va_list ap;
  va_start(ap, format);
  char *p;

  char* sVal;
  int iVal;
  double dVal;

  for (p = format; *p; p++)
  {
    if (*p != '%')
    {
      fprintf(fp, "%c", *p);
      continue;
    }
    switch (*++p)
    {
      case 'd': case 'u':
        iVal = va_arg(ap, int);
        fprintf(fp, "%d", iVal);
        break;
      case 'f':
        dVal = va_arg(ap, double);
        fprintf(fp, "%f", dVal);
        break;
      case 's':
        for (sVal = va_arg(ap, char*); *sVal; sVal++)
          fprintf(fp, "%c", *sVal);
        break;
      default:
        fprintf(fp, "%c", *p);
        break;
    }
  }
  va_end(ap);
}
