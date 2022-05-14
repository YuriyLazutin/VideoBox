#include "pump.h"

void send_not_found(const int conn)
{
  char* not_found_response =
    "HTTP/1.0 404 Not Found\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Not Found</h1>\n"
    "  <p>The requested URL was not found.</p>\n"
    " </body>\n"
    "</html>\n";

  write(conn, not_found_response, strlen(not_found_response));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", not_found_response);
}

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

static char* head_template =
"HTTP/1.1 200 OK\n"
"Accept-Ranges: bytes\n"
"Content-Length: %lu\n"
"Content-Type: %s\n"
"\n";

int pump(int conn, const char* params)
{
  int rc = EXIT_SUCCESS, rc2;

  char* ppar = strstr(params, "?pump=") + strlen("?pump=");
  ssize_t length = strlen(ppar);

  char *flags, *file_name, *mime_type;

  if (length < SIG_SIZE + ID_SIZE + FLAGS_SIZE)
    rc = EXIT_FAILURE;

  if (rc == EXIT_SUCCESS)
  {
    flags = strndup(ppar + SIG_SIZE + ID_SIZE, FLAGS_SIZE);
    if (flags == NULL)
      rc = EXIT_FAILURE;
  }

  if (rc == EXIT_SUCCESS)
  {
    switch (flags[0])
    {
      case 'f':
        if (flags[1] == '4')
        {
          file_name = strdup("video.mp4");
          mime_type = strdup("video/mp4");
        }
        else if (flags[1] == 'w')
        {
          file_name = strdup("video.webm");
          mime_type = strdup("video/webm");
        }
        break;
      case 'p':
        if (flags[1] == 'p')
        {
          file_name = strdup("trumb.png");
          mime_type = strdup("image/png");
        }
        else if (flags[1] == 'j')
        {
          file_name = strdup("trumb.jpg");
          mime_type = strdup("image/jpeg");
        }
        else if (flags[1] == 'w')
        {
          file_name = strdup("trumb.webp");
          mime_type = strdup("image/webp");
        }
        break;
      default:
        rc = EXIT_FAILURE;
    }
    if (file_name == NULL || mime_type == NULL)
      rc = EXIT_FAILURE;

  }


  char *sig, *id;
  if (rc == EXIT_SUCCESS)
  {
    sig = strndup(ppar, SIG_SIZE);
    id = strndup(ppar + SIG_SIZE, ID_SIZE);
    if (sig == NULL || id == NULL)
      rc = EXIT_FAILURE;
  }

  char *file_path;
  if (rc == EXIT_SUCCESS)
  {
    length = strlen(showboard_dir) + SIG_SIZE + 1 + ID_SIZE + 1 + strlen(file_name) + 1;
    if (length >= PATH_MAX)
        rc = EXIT_FAILURE;
  }

  if (rc == EXIT_SUCCESS)
  {
    file_path = malloc(length);
    if (!file_path)
      rc = EXIT_FAILURE;
  }

  if (rc == EXIT_SUCCESS)
  {
    rc2 = snprintf(file_path, length, "%s%s/%s/%s", showboard_dir, sig, id, file_name);
    if (rc2 < 0 || rc2 >= length)
      rc = EXIT_FAILURE;
  }

  int read_fd;
  struct stat file_info;

  if (rc == EXIT_SUCCESS)
  {
    read_fd = open(file_path, O_RDONLY);
    if (read_fd == -1)
      rc = EXIT_FAILURE;
  }

  if (rc == EXIT_SUCCESS)
  {
    rc2 = fstat(read_fd, &file_info);
    if (rc2 == -1)
      rc = EXIT_FAILURE;
  }

  char buf[STANDARD_BUFFER_SIZE];
  if (rc == EXIT_SUCCESS)
  {
    length = snprintf(buf, STANDARD_BUFFER_SIZE, head_template, file_info.st_size, mime_type);
    if (length < 0 || length >= STANDARD_BUFFER_SIZE)
      rc = EXIT_FAILURE;
  }

  if (rc == EXIT_SUCCESS)
  {
    write(conn, buf, length);
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", buf);
  }

  if (rc == EXIT_SUCCESS)
  {
    off_t offset = 0;
    rc2 = sendfile(conn, read_fd, &offset, file_info.st_size);
    if (rc2 == -1)
      rc = EXIT_FAILURE;

    // Debug file output
    while ((rc2 = read(read_fd, buf, STANDARD_BUFFER_SIZE - 1)) > 0)
    {
      buf[rc2] = '\0';
      fprintf(stdout, "%s", buf);
    }
  }

  close(read_fd);
  if (file_path)
    free(file_path);
  if (id)
    free(id);
  if (sig)
    free(sig);
  if (mime_type)
    free(mime_type);
  if (file_name)
    free(file_name);
  if (flags)
    free(flags);

  if (rc != EXIT_SUCCESS)
    send_not_found(conn);

  return rc;
}
