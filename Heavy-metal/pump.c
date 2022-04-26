#include "pump.h"

/* PNG package
HTTP/1.1 200 OK
Date: Wed, 07 Jan 1970 17:17:24 GMT
Server: Apache/2.4.7 (Ubuntu)
Last-Modified: Wed, 07 Jan 1970 17:17:25 GMT
ETag: W/"12a4-5dc9af5796040"
Accept-Ranges: bytes
Content-Length: 4772
Connection: close
Content-Type: image/png

content of png...
*/

/* MP4 video
HTTP/1.1 200 OK
Date: Wed, 07 Jan 1970 17:32:58 GMT
Server: Apache/2.4.7 (Ubuntu)
Last-Modified: Wed, 07 Jan 1970 17:32:58 GMT
ETag: W/"26-5dc9b3fae9180"
Accept-Ranges: bytes
Content-Length: 38
Connection: close
Content-Type: video/mp4

content of mp4...
*/

static char* page =
"HTTP/1.1 200 OK\n"
"Accept-Ranges: bytes\n"
"Content-Length: %d\n"
"Content-Type: video/mp4\n"
"\n";
// content of png...

int pump(int conn, const char* params)
{
  int rc = EXIT_SUCCESS;

  write(conn, page, strlen(page));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", page);

  return rc;
}
