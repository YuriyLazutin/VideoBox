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

int pump(int conn, const char* params)
{
  int rc = EXIT_SUCCESS, rc2;
  size_t length = strlen(params);

  char *index, *sig, *ext, *mime_type, *file_name;

  if (length < INDEX_SIZE + SIG_SIZE)
    rc = EXIT_FAILURE;

  if (rc == EXIT_SUCCESS)
  {
    index = strndup(params, INDEX_SIZE);
    if (index == NULL)
      rc = EXIT_FAILURE;

    switch (index[0])
    {
      case 'a':
        ext = strdup("mp4");
        mime_type = strdup("video/mp4");
        break;
      case 'b':
        ext = strdup("webm");
        mime_type = strdup("video/webm");
        break;
      case 'c':
        ext = strdup("png");
        mime_type = strdup("image/png");
        break;
      case 'd':
        ext = strdup("webp");
        mime_type = strdup("image/webp");
        break;
      default:
        rc = EXIT_FAILURE;
    }
  }

  if (rc == EXIT_SUCCESS)
  {
    sig = strndup(params + INDEX_SIZE, SIG_SIZE);
    if (sig == NULL)
      rc = EXIT_FAILURE;
  }

  if (rc == EXIT_SUCCESS)
  {
    length = strlen(server_dir) + 1 + strlen(sig) + 1 + 5 + strlen(ext);
    file_name = (char*)malloc( (length+1)*sizeof(char) );
    rc2 = snprintf(file_name, length + 1, "%s/%s/%4s.%s", server_dir, sig, index + FLAGS_SIZE, ext);
  }

  int read_fd;
  struct stat file_info;

  read_fd = open(file_name, O_RDONLY);
  if (read_fd == -1)
    rc = EXIT_FAILURE;

  if (rc == EXIT_SUCCESS)
  {
    rc2 = fstat(read_fd, &file_info);
    if (rc2 == -1) // не удалось открыть файл или прочитать данные из него
      rc = EXIT_FAILURE;
  }

  if (rc == EXIT_SUCCESS)
  {
    char buf[256];
    length = sizeof(buf);

    length = snprintf(buf, sizeof(buf), "%s%s", "HTTP/1.1 200 OK\n", "Accept-Ranges: bytes\n");
    write(conn, buf, length);
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", buf);

    length = snprintf(buf, sizeof(buf), "Content-Length: lu\n", file_info.st_size);
    write(conn, buf, length);
    fprintf(stdout, "%s", buf);

    length = snprintf(buf, sizeof(buf), "Content-Type: %s\n\n", "video/mp4");
    write(conn, buf, length);
    fprintf(stdout, "%s", buf);

    off_t offset = 0;
    rc2 = sendfile(conn, read_fd, &offset, file_info.st_size);
    if (rc2 == -1) // При отправке файла произошла ошибка
      rc = EXIT_FAILURE;

    // Debug file output
    while ((rc2 = read(read_fd, buf, sizeof(buf)-1)) > 0)
    {
      buf[rc2] = '\0';
      fprintf(stdout, "%s", buf);
    }
  }

  close(read_fd);
  if (index)
    free(index);
  if (sig)
    free(sig);
  if (file_name)
    free(file_name);

  if (rc != EXIT_SUCCESS)
    send_not_found(conn);

  return rc;
}
