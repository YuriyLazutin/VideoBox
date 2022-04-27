#include "showboard.h"

static char* page_begin =
"\n"
"\n";

static char* page_end =
"\n"
"\n";

int showboard(int conn, const char* params)
{
  write(conn, page_begin, strlen(page_begin));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", page_begin);

  int rc = EXIT_SUCCESS, rc2;
  ssize_t length = strlen(params);
  char dir_name[PATH_MAX];
  DIR* dir;
  struct dirent* entry;
  struct stat st;

  if (rc == EXIT_SUCCESS)
  {
    length = strlen(server_dir) + strlen("/showboard/");
    rc2 = snprintf(dir_name, length + 1, "%s/showboard/", server_dir);
    if (rc2 < 0 || rc2 >= length + 1)
      rc = EXIT_FAILURE;
  }

  dir = opendir(dir_name);
  while ((entry = readdir(dir)) != NULL)
  {
    strncpy(dir_name + length, entry->d_name, PATH_MAX - length);
    lstat(dir_name, &st);
    if (S_ISLNK(st.st_mode)) // symbolic link
    {
    }
    else if (S_ISDIR(st.st_mode)) // directory
    {
    }
    else if (S_ISREG(st.st_mode)) // regular file
    {
    }
  }

  closedir(dir);


  write(conn, page_end, strlen(page_end));
  fprintf(stdout, "%s", page_end);

  return rc;
}
