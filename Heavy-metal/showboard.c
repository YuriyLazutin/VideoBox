#include "showboard.h"

static char* page =
"\n"
"\n";

int showboard(int conn, const char* params)
{
  int rc = EXIT_SUCCESS;

  write(conn, page, strlen(page));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", page);

  return rc;
}
