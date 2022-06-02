#include "pump.h"

void send_not_found(const int conn);
void send_range_not_satisfiable(const int conn, long size);

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

static char* partial_head_template =
"HTTP/1.1 206 Partial Content\n"
"Content-Type: %s\n"
"Content-Range: bytes %lu-%lu/%lu\n"
"Content-Length: %lu\n"
"\n";

static char* multipart_head =
"HTTP/1.1 206 Partial Content\n"
"Content-Type: multipart/byteranges; boundary=X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X\n"
"Content-Length: %lu\n"
"\n";

static char* multipart_part_head_template =
"\n--X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X\n"
"Content-Type: %s\n"
"Content-Range: bytes %lu-%lu/%lu\n"
"\n";

static char* multipart_end =
"\n--X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X0X--\n";

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
    {
      #ifndef NDEBUG
        log_print("pump->fstat: %s\n", strerror(errno));
      #endif
      rc = EXIT_FAILURE;
    }
  }

  char buf[STANDARD_BUFFER_SIZE];
  if (!blocks) // send file as one block with 200 responce
  {
    if (rc == EXIT_SUCCESS)
    {
      length = snprintf(buf, STANDARD_BUFFER_SIZE, head_template, file_info.st_size, mime_type);
      if (length < 0 || length >= STANDARD_BUFFER_SIZE)
        rc = EXIT_FAILURE;
    }

    if (rc == EXIT_SUCCESS)
    {
      ssize_t bytes_wrote = write_block(conn, buf, length);
      #ifndef NDEBUG
        if (bytes_wrote == length)
        {
          log_print("pump: Sended to client:\n");
          log_print("%s", buf);
        }
        else if (bytes_wrote > 0)
        {
          log_print("pump: partially sended to client (%ld bytes sended):\n", bytes_wrote);
          log_print("%s", buf);
        }
        else if (bytes_wrote == 0)
          log_print("pump: Tried to send Ok response but 0 bytes sended.\n");
        else if (bytes_wrote == -1 * TIME_OUT)
          log_print("pump: Tried to send Ok response but TIME_OUT occursed.\n");
        else if (bytes_wrote == -1 * CONNECTION_CLOSED)
          log_print("pump: Tried to send Ok response but CONNECTION_CLOSED.\n");
        else if (bytes_wrote == -1 * POLL_ERROR)
          log_print("pump: Tried to send Ok response but POLL_ERROR occursed.\n");
        else if (bytes_wrote == -1 * WRITE_BLOCK_ERROR)
          log_print("pump: Tried to send Ok response but WRITE_BLOCK_ERROR occursed.\n");
      #endif
    }

    if (rc == EXIT_SUCCESS)
    {
      off_t offset = 0;
      ssize_t bytes_sent = sendfile(conn, read_fd, &offset, file_info.st_size);
      if (bytes_sent == -1)
      {
        rc = EXIT_FAILURE;
        #ifndef NDEBUG
          log_print("pump: Error! sendfile filed (%s)\n", strerror(errno));
        #endif
      }
      #ifndef NDEBUG
      else if (bytes_sent != file_info.st_size)
        log_print("pump: Warning! sendfile sended less bytes then requested (%ld < %ld)\n", bytes_sent, file_info.st_size);
      #endif

      // Debug file output
      #ifndef NDEBUG
        log_print("File content here...\n");
        /*while ((rc2 = read(read_fd, buf, STANDARD_BUFFER_SIZE - 1)) > 0)
        {
          buf[rc2] = '\0';
          log_print("%s", buf);
        }*/
      #endif
    }
  }
  else
  {
    size_t send_cnt;

    struct block_range *pb = blocks;
    while (pb && rc == EXIT_SUCCESS)
    {
      #ifndef NDEBUG
        log_print("pump: Process range (%ld, %ld)\n", pb->start, pb->end);
      #endif

      if (pb->start < 0)
        pb->start = file_info.st_size - -pb->start;

      if (pb->end == 0 || pb->end >= file_info.st_size)
        pb->end = file_info.st_size - 1;
      else if (pb->end < 0)
        pb->end = file_info.st_size - -pb->end;

      if (pb->start < 0 || // offset > file_size
          pb->end < 0 ||  // offset > file_size
          pb->start > pb->end // invalid range
         )
      {
        #ifndef NDEBUG
          log_print("pump: Detected invalid range in request (start_pos = %ld, end_pos = %ld, file_size = %lu)\n", pb->start, pb->end, file_info.st_size);
        #endif
        send_range_not_satisfiable(conn, file_info.st_size);
        rc = EXIT_FAILURE;
      }

      // Collapse ranges here if needed
      // ...

      pb = pb->pNext;
    }

    // Prepare header
    if (rc == EXIT_SUCCESS)
    {
      if (!blocks->pNext) // Range bytes requested. send file as one block with 206 responce
      {
        send_cnt = blocks->end - blocks->start + 1;
        length = snprintf(buf, STANDARD_BUFFER_SIZE, partial_head_template, mime_type, blocks->start, blocks->end, file_info.st_size, send_cnt);
      }
      else // A few range bytes requested. send file as few blocks with 206 responce
        length = snprintf(buf, STANDARD_BUFFER_SIZE, multipart_head, file_info.st_size);

      if (length < 0 || length >= STANDARD_BUFFER_SIZE)
        rc = EXIT_FAILURE;
    }

    // Send header
    if (rc == EXIT_SUCCESS)
    {
      ssize_t bytes_wrote = write_block(conn, buf, length);
      #ifndef NDEBUG
        if (bytes_wrote == length)
        {
          log_print("pump: Sended to client:\n");
          log_print("%s", buf);
        }
        else if (bytes_wrote > 0)
        {
          log_print("pump: partially sended to client (%ld bytes sended):\n", bytes_wrote);
          log_print("%s", buf);
        }
        else if (bytes_wrote == 0)
          log_print("pump: Tried to send 206 Partial Content response but 0 bytes sended.\n");
        else if (bytes_wrote == -1 * TIME_OUT)
          log_print("pump: Tried to send 206 Partial Content response but TIME_OUT occursed.\n");
        else if (bytes_wrote == -1 * CONNECTION_CLOSED)
          log_print("pump: Tried to send 206 Partial Content response but CONNECTION_CLOSED.\n");
        else if (bytes_wrote == -1 * POLL_ERROR)
          log_print("pump: Tried to send 206 Partial Content response but POLL_ERROR occursed.\n");
        else if (bytes_wrote == -1 * WRITE_BLOCK_ERROR)
          log_print("pump: Tried to send 206 Partial Content response but WRITE_BLOCK_ERROR occursed.\n");
      #endif

    }

    // Send file parts
    if (rc == EXIT_SUCCESS)
    {
      if (!blocks->pNext) // send file as one block
      {
        off_t offset = blocks->start;
        ssize_t bytes_sent = sendfile(conn, read_fd, &offset, send_cnt);
        if (bytes_sent == -1)
        {
          rc = EXIT_FAILURE;
          #ifndef NDEBUG
            log_print("pump: Error! sendfile filed (%s)\n", strerror(errno));
          #endif
        }
        #ifndef NDEBUG
        else if (bytes_sent != send_cnt)
          log_print("pump: Warning! sendfile sended less bytes then requested (%ld < %ld)\n", bytes_sent, send_cnt);
        #endif
      }
      else // send file as few blocks
      {
        pb = blocks;
        while (pb && rc == EXIT_SUCCESS)
        {
          send_cnt = pb->end - pb->start + 1;
          length = snprintf(buf, STANDARD_BUFFER_SIZE, multipart_part_head_template, mime_type, pb->start, pb->end, file_info.st_size);
          if (length < 0 || length >= STANDARD_BUFFER_SIZE)
            rc = EXIT_FAILURE;

          // Send multipart header
          if (rc == EXIT_SUCCESS)
          {
            ssize_t bytes_wrote = write_block(conn, buf, length);
            #ifndef NDEBUG
              if (bytes_wrote == length)
                log_print("%s", buf);
              else if (bytes_wrote > 0)
              {
                log_print("pump: partially sended to client (%ld bytes sended):\n", bytes_wrote);
                log_print("%s", buf);
              }
              else if (bytes_wrote == 0)
                log_print("pump: Tried to send multipart part but 0 bytes sended.\n");
              else if (bytes_wrote == -1 * TIME_OUT)
                log_print("pump: Tried to send multipart part but TIME_OUT occursed.\n");
              else if (bytes_wrote == -1 * CONNECTION_CLOSED)
                log_print("pump: Tried to send multipart part but CONNECTION_CLOSED.\n");
              else if (bytes_wrote == -1 * POLL_ERROR)
                log_print("pump: Tried to send multipart part but POLL_ERROR occursed.\n");
              else if (bytes_wrote == -1 * WRITE_BLOCK_ERROR)
                log_print("pump: Tried to send multipart part but WRITE_BLOCK_ERROR occursed.\n");
            #endif
          }

          // Send bytes range
          if (rc == EXIT_SUCCESS)
          {
            off_t offset = pb->start;
            ssize_t bytes_sent = sendfile(conn, read_fd, &offset, send_cnt);
            if (bytes_sent == -1)
            {
              rc = EXIT_FAILURE;
              #ifndef NDEBUG
                log_print("pump: Error! sendfile filed (%s)\n", strerror(errno));
              #endif
            }
            #ifndef NDEBUG
            else if (bytes_sent != send_cnt)
              log_print("pump: Warning! sendfile sended less bytes then requested (%ld < %ld)\n", bytes_sent, send_cnt);
            #endif
          }
          pb = pb->pNext;
        }

         // Send multipart_end
        if (rc == EXIT_SUCCESS)
        {
          length = strlen(multipart_end);
          ssize_t bytes_wrote = write_block(conn, multipart_end, length);
          #ifndef NDEBUG
            if (bytes_wrote == length)
              log_print("%s", multipart_end);
            else if (bytes_wrote > 0)
            {
              log_print("pump: partially sended to client (%ld bytes sended):\n", bytes_wrote);
              log_print("%s", multipart_end);
            }
            else if (bytes_wrote == 0)
              log_print("pump: Tried to send multipart_end but 0 bytes sended.\n");
            else if (bytes_wrote == -1 * TIME_OUT)
              log_print("pump: Tried to send multipart_end but TIME_OUT occursed.\n");
            else if (bytes_wrote == -1 * CONNECTION_CLOSED)
              log_print("pump: Tried to send multipart_end but CONNECTION_CLOSED.\n");
            else if (bytes_wrote == -1 * POLL_ERROR)
              log_print("pump: Tried to send multipart_end but POLL_ERROR occursed.\n");
            else if (bytes_wrote == -1 * WRITE_BLOCK_ERROR)
              log_print("pump: Tried to send multipart_end but WRITE_BLOCK_ERROR occursed.\n");
          #endif
        }
      }
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

  int len = strlen(not_found_response);
  ssize_t bytes_wrote = write_block(conn, not_found_response, len);

  #ifndef NDEBUG
    if (bytes_wrote == len)
    {
      log_print("send_not_found: Sended to client:\n");
      log_print("%s", not_found_response);
    }
    else if (bytes_wrote > 0)
    {
      log_print("send_not_found: Not found response partially sended to client (%ld bytes sended):\n", bytes_wrote);
      log_print("%s", not_found_response);
    }
    else if (bytes_wrote == 0)
      log_print("send_not_found: Tried to send Not found response but 0 bytes sended.\n");
    else if (bytes_wrote == -1 * TIME_OUT)
      log_print("send_not_found: Tried to send Not found response but TIME_OUT occursed.\n");
    else if (bytes_wrote == -1 * CONNECTION_CLOSED)
      log_print("send_not_found: Tried to send Not found response but CONNECTION_CLOSED.\n");
    else if (bytes_wrote == -1 * POLL_ERROR)
      log_print("send_not_found: Tried to send Not found response but POLL_ERROR occursed.\n");
    else if (bytes_wrote == -1 * WRITE_BLOCK_ERROR)
      log_print("send_not_found: Tried to send Not found response but WRITE_BLOCK_ERROR occursed.\n");
  #endif
}

void send_range_not_satisfiable(const int conn, long size)
{
  char* range_not_satisfiable_response_template =
    "HTTP/1.1 416 Range Not Satisfiable\n"
    "Content-Range: bytes */%lu\n";

  int len = strlen(range_not_satisfiable_response_template);
  int bufsize = len + 64;
  char* buf = malloc(bufsize);
  if (!buf)
  {
    #ifndef NDEBUG
      log_print("send_range_not_satisfiable: malloc failed.\n");
    #endif
    return;
  }

  len = snprintf(buf, bufsize, range_not_satisfiable_response_template, size);
  if (len < 0 || len >= bufsize)
  {
    #ifndef NDEBUG
      log_print("send_range_not_satisfiable: snprintf failed.\n");
    #endif
    return;
  }

  len = strlen(buf);
  ssize_t bytes_wrote = write_block(conn, buf, len);

  #ifndef NDEBUG
    if (bytes_wrote == len)
    {
      log_print("send_range_not_satisfiable: Sended to client:\n");
      log_print("%s", buf);
    }
    else if (bytes_wrote > 0)
    {
      log_print("send_range_not_satisfiable: partially sended to client (%ld bytes sended):\n", bytes_wrote);
      log_print("%s", buf);
    }
    else if (bytes_wrote == 0)
      log_print("send_range_not_satisfiable: Tried to send Range Not Satisfiable response but 0 bytes sended.\n");
    else if (bytes_wrote == -1 * TIME_OUT)
      log_print("send_range_not_satisfiable: Tried to send Range Not Satisfiable response but TIME_OUT occursed.\n");
    else if (bytes_wrote == -1 * CONNECTION_CLOSED)
      log_print("send_range_not_satisfiable: Tried to send Range Not Satisfiable response but CONNECTION_CLOSED.\n");
    else if (bytes_wrote == -1 * POLL_ERROR)
      log_print("send_range_not_satisfiable: Tried to send Range Not Satisfiable response but POLL_ERROR occursed.\n");
    else if (bytes_wrote == -1 * WRITE_BLOCK_ERROR)
      log_print("send_range_not_satisfiable: Tried to send Range Not Satisfiable response but WRITE_BLOCK_ERROR occursed.\n");
  #endif
}
