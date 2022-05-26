#include "logger.h"

void log_init(char* path)
{
  int errcode;
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

  log_fd = open(path, O_WRONLY | O_CREAT, mode);
  if (log_fd == -1)
  {
    errcode = errno;
    fprintf(stderr, "Error in function \"log_init\": %s\n", strerror(errcode));

    if (errcode == ENOENT) // A directory component in pathname does not exist or is a dangling symbolic link.
    {
      char* dir_path = strdup(path);
      if (dir_path == NULL)
      {
        log_fd = STDERR_FILENO;
        return;
      }

      ssize_t length = strlen(dir_path);
      while (length > 0 && dir_path[length - 1] != '/')
        length--;
      dir_path[length] = '\0';

      mode_t dir_mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP;
      int dir_fd = mkdir(dir_path, dir_mode);
      free(dir_path);

      if (dir_fd == -1)
      {
        errcode = errno;
        fprintf(stderr, "Error in function \"log_init\" during create log directory: %s\n", strerror(errcode));
        log_fd = STDERR_FILENO;
        return;
      }
      else
        fprintf(stderr, "\"log_init\". Create log directory: successfully\n");

      log_fd = open(path, O_WRONLY | O_CREAT, mode);
      if (log_fd == -1)
      {
        errcode = errno;
        fprintf(stderr, "Error in function \"log_init\". Can't create log file: %s\n", strerror(errcode));
        log_fd = STDERR_FILENO;
        return;
      }
    }
  }
}

void server_log_init()
{
  char server_log_path[PATH_MAX];
  ssize_t length;
  int iError = NO_ERRORS;

  length = readlink("/proc/self/exe", server_log_path, PATH_MAX - 1);
  if (length == -1)
    iError = READ_LINK_ERROR;

  if (iError == NO_ERRORS)
  {
    while (length > 0 && server_log_path[length - 1] != '/') length--;
    if (!length)
      iError = PATH_INVALID;
  }

  if (iError == NO_ERRORS)
  {
    if (length + strlen("log/server.log") + 1 > PATH_MAX)
      iError = PATH_OVERFLOW;
    else
      strcpy(server_log_path + length, "log/server.log");
  }

  if (iError == NO_ERRORS)
    log_init(server_log_path);
  else
    log_fd = STDERR_FILENO;
}

void connection_log_init(int connection_id)
{
  char connection_log_path[PATH_MAX];
  ssize_t length;
  int iError = NO_ERRORS;

  length = readlink("/proc/self/exe", connection_log_path, PATH_MAX - 1);
  if (length == -1)
    iError = READ_LINK_ERROR;

  if (iError == NO_ERRORS)
  {
    while (length > 0 && connection_log_path[length - 1] != '/') length--;
    if (!length)
      iError = PATH_INVALID;
  }

  if (iError == NO_ERRORS)
  {
    int len2 = snprintf(connection_log_path + length, PATH_MAX - length, "log/%05d.log", connection_id);
    if (length + len2 + 1 > PATH_MAX)
      iError = PATH_OVERFLOW;
  }

  if (iError == NO_ERRORS)
    log_init(connection_log_path);
  else
    log_fd = STDERR_FILENO;
}

void log_close()
{
  if (log_fd != STDERR_FILENO && log_fd != STDOUT_FILENO)
    close(log_fd);
}

void log_print(char* format, ...)
{
  FILE* fp = fdopen(log_fd, "w");
  if (fp == NULL)
  {
    fprintf(stderr, "log_print: Bad fd->fp convertion! Will used stderr");
    fp = stderr;
  }

  va_list ap;
  va_start(ap, format);
  vfprintf(fp, format, ap);
  fflush(fp);
  //fsync(log_fd);
  va_end(ap);
}
