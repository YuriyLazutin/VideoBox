#include "pump.h"

void send_not_found(const int conn);
void send_range_not_satisfiable(const int conn, long size);

static char* head_template =
"HTTP/1.1 200 OK\n"
"Accept-Ranges: bytes\n"
"Content-Length: %lu\n"
"Content-Type: %s\n"
"\n";

static char* partial_head_template =
"HTTP/1.1 206 Partial Content\n"
"Content-Type: %s\n"
"Content-Range: bytes %lu-%lu/%lu\n"
"Content-Length: %lu\n"
"\n";

static char* multipart_head =
"HTTP/1.1 206 Partial Content\n"
"Content-Type: multipart/byteranges; boundary=X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X\n"
"Content-Length: %lu\n";

static char* multipart_part_head_template =
"\n--X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X\n"
"Content-Type: %s\n"
"Content-Range: bytes %lu-%lu/%lu\n"
"\n";

static char* multipart_end =
"\n--X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X--\n";

int pump(int conn, const char* params)
{
  int rc = NO_ERRORS, rc2;

  char* ppar = strstr(params, "?pump=") + strlen("?pump=");
  ssize_t length = strlen(ppar);
  if (length < SIG_SIZE + ID_SIZE + FLAGS_SIZE)
    rc = BAD_REQUEST;

  char *flags = NULL, *file_name = NULL, *mime_type = NULL;

  if (rc == NO_ERRORS)
  {
    flags = strndup(ppar + SIG_SIZE + ID_SIZE, FLAGS_SIZE);
    if (flags == NULL)
      rc = MALLOC_FAILED;
  }

  if (rc == NO_ERRORS)
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
        rc = BAD_REQUEST;
    }
    if (file_name == NULL || mime_type == NULL)
      rc = MALLOC_FAILED;
  }

  char *sig = NULL, *id = NULL;
  if (rc == NO_ERRORS)
  {
    sig = strndup(ppar, SIG_SIZE);
    id = strndup(ppar + SIG_SIZE, ID_SIZE);
    if (sig == NULL || id == NULL)
      rc = MALLOC_FAILED;
  }

  char *file_path = NULL;
  if (rc == NO_ERRORS)
  {
    length = showboard_dir_length + SIG_SIZE + 1 + ID_SIZE + 1 + strlen(file_name) + 1;
    if (length >= PATH_MAX)
      rc = PATH_OVERFLOW;

    if (rc == NO_ERRORS)
    {
      file_path = malloc(length);
      if (!file_path)
        rc = MALLOC_FAILED;
    }

    if (rc == NO_ERRORS)
    {
      rc2 = snprintf(file_path, length, "%s%s/%s/%s", showboard_dir, sig, id, file_name);
      if (rc2 < 0 || rc2 >= length)
        rc = PATH_INVALID;
    }
  }

  int read_fd, read_fd_opened = 0;
  struct stat file_info;

  if (rc == NO_ERRORS)
  {
    read_fd = open(file_path, O_RDONLY);
    if (read_fd == -1)
      rc = OPEN_FILE_ERROR;
    else
      read_fd_opened = 1;
  }

  if (rc == NO_ERRORS)
  {
    rc2 = fstat(read_fd, &file_info);
    if (rc2 == -1)
      rc = STAT_FAILED;
  }

  char buf[STANDARD_BUFFER_SIZE];
  if (rc == NO_ERRORS)
  {
    if (!blocks)
    {
      if (rc == NO_ERRORS)
      {
        length = snprintf(buf, STANDARD_BUFFER_SIZE, head_template, file_info.st_size, mime_type);
        if (length < 0 || length >= STANDARD_BUFFER_SIZE)
          rc = BUFFER_OVERFLOW;
      }

      if (rc == NO_ERRORS)
        write_block(conn, buf, length);

      if (rc == NO_ERRORS)
      {
        off_t offset = 0;
        ssize_t bytes_sent = sendfile(conn, read_fd, &offset, file_info.st_size);
        if (bytes_sent == -1)
          rc = WRITE_BLOCK_ERROR;
      }

      if (rc == NO_ERRORS)
        write_block(conn, "\n\n", 2);
    }
    else
    {
      size_t send_cnt;

      struct block_range *pb = blocks;
      while (pb && rc == NO_ERRORS)
      {
        if (pb->start < 0)
          pb->start = file_info.st_size - -pb->start;

        if (pb->end == 0 || pb->end >= file_info.st_size)
          pb->end = file_info.st_size - 1;
        else if (pb->end < 0)
          pb->end = file_info.st_size - -pb->end;

        if (pb->start < 0 || pb->end < 0 || pb->start > pb->end)
          rc = INVALID_RANGE;

        pb = pb->pNext;
      }

      if (rc == NO_ERRORS)
      {
        if (!blocks->pNext)
        {
          send_cnt = blocks->end - blocks->start + 1;
          length = snprintf(buf, STANDARD_BUFFER_SIZE, partial_head_template, mime_type, blocks->start, blocks->end, file_info.st_size, send_cnt);
        }
        else
          length = snprintf(buf, STANDARD_BUFFER_SIZE, multipart_head, file_info.st_size);

        if (length < 0 || length >= STANDARD_BUFFER_SIZE)
          rc = BUFFER_OVERFLOW;
      }

      if (rc == NO_ERRORS)
        write_block(conn, buf, length);

      if (rc == NO_ERRORS)
      {
        if (!blocks->pNext)
        {
          off_t offset = blocks->start;
          ssize_t bytes_sent = sendfile(conn, read_fd, &offset, send_cnt);
          if (bytes_sent == -1)
            rc = WRITE_BLOCK_ERROR;

          if (rc == NO_ERRORS)
            bytes_sent = write_block(conn, "\n\n", 2);
        }
        else
        {
          pb = blocks;
          while (pb && rc == NO_ERRORS)
          {
            send_cnt = pb->end - pb->start + 1;
            length = snprintf(buf, STANDARD_BUFFER_SIZE, multipart_part_head_template, mime_type, pb->start, pb->end, file_info.st_size);
            if (length < 0 || length >= STANDARD_BUFFER_SIZE)
              rc = BUFFER_OVERFLOW;

            if (rc == NO_ERRORS)
              write_block(conn, buf, length);

            if (rc == NO_ERRORS)
            {
              off_t offset = pb->start;
              ssize_t bytes_sent = sendfile(conn, read_fd, &offset, send_cnt);
              if (bytes_sent == -1)
                rc = WRITE_BLOCK_ERROR;
            }
            pb = pb->pNext;
          }

          if (rc == NO_ERRORS)
          {
            length = strlen(multipart_end);
            write_block(conn, multipart_end, length);
          }
        }
      }
    }
  }

  if (read_fd_opened)
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

  if (rc == INVALID_RANGE)
    send_range_not_satisfiable(conn, file_info.st_size);
  else if (rc != NO_ERRORS)
    send_not_found(conn);

  return rc;
}

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
    "</html>\n"
    "\n";

  ssize_t length = strlen(not_found_response);
  write_block(conn, not_found_response, length);
}

void send_range_not_satisfiable(const int conn, long size)
{
  char* range_not_satisfiable_response_template =
    "HTTP/1.1 416 Range Not Satisfiable\n"
    "Content-Range: bytes */%lu\n"
    "\n";

  int len = strlen(range_not_satisfiable_response_template);
  int bufsize = len + 64;
  char* buf = malloc(bufsize);
  if (!buf)
    return;

  len = snprintf(buf, bufsize, range_not_satisfiable_response_template, size);
  if (len < 0 || len >= bufsize)
    return;

  write_block(conn, buf, len);
}
