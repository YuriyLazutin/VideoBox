#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // readlink
#include <errno.h> // errno
#include <string.h> // strcpy, strerror
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include "defines.h"
#include "common.h"
#include "player.h"
#include "pump.h"
#include "showboard.h"

void clean_up_child_process();
int close_descriptor(int d);
int process_connection(int conn);
int process_get(int conn, const char* page);
ssize_t read_block(const int conn, char* buf, const ssize_t buf_size);
ssize_t write_block(const int conn, const char* buf, const ssize_t count);
int skip_spaces(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
char* read_word(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
char* read_line(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
int parse_param_line(char* str);
char* find_str_ncase(const char* buf, const char* str);
void send_bad_request(const int conn);
void send_request_timeout(const int conn);
void send_bad_method(const int conn);
char* showboard_dir;
ssize_t showboard_dir_length;
int vbx_errno;

struct block_range* blocks;

int main()
{
  showboard_dir = malloc(PATH_MAX);
  if (!showboard_dir)
    return EXIT_FAILURE;
  showboard_dir_length = readlink("/proc/self/exe", showboard_dir, PATH_MAX - 1);
  if (showboard_dir_length == -1)
    return EXIT_FAILURE;
  while (showboard_dir_length > 0 && showboard_dir[showboard_dir_length - 1] != '/') showboard_dir_length--;
  if (!showboard_dir_length)
    return EXIT_FAILURE;
  if (showboard_dir_length + strlen("showboard/") + 1 > PATH_MAX)
    return EXIT_FAILURE;
  strcpy(showboard_dir + showboard_dir_length, "showboard/");
  showboard_dir_length += strlen("showboard/");
  showboard_dir = realloc(showboard_dir, showboard_dir_length + 1);
  if (!showboard_dir)
    return EXIT_FAILURE;

  struct sockaddr_in server_address;
  struct sockaddr_in remote_address;
  socklen_t address_length = sizeof(server_address);
  memset(&server_address, 0, address_length);
  server_address.sin_family = AF_INET;
  server_address.sin_port = (uint16_t)htons(SERVER_PORT);
  struct in_addr local_address;
  local_address.s_addr = INADDR_ANY;
  server_address.sin_addr = local_address;

  struct sigaction sigchld_action;
  memset(&sigchld_action, 0, sizeof(sigchld_action));
  sigchld_action.sa_handler = &clean_up_child_process;
  sigaction(SIGCHLD, &sigchld_action, NULL);

  int server_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (server_socket == -1)
  {
    fprintf(stderr, "Error in function \"socket\": %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  const int on = 1;
  int rc = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  rc = bind(server_socket, (const struct sockaddr *)&server_address, sizeof(server_address));
  if (rc != 0)
  {
    fprintf(stderr, "Error in function \"bind\": %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  rc = listen(server_socket, SERVER_LISTEN_BACKLOG);
  if (rc != 0)
  {
    fprintf(stderr, "Error in function \"listen\": %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  do
  {
    address_length = sizeof(remote_address);
    memset(&remote_address, 0, address_length);
    int connection = accept(server_socket, (struct sockaddr *)&remote_address, &address_length);
    if (connection == -1)
    {
      switch (errno)
      {
        case EINTR:
          continue;
        case EBADF:
        case EINVAL:
        case ENOTSOCK:
        case EOPNOTSUPP:
          fprintf(stderr, "Critical error in function \"accept\": %s\n", strerror(errno));
          rc = EXIT_FAILURE;
          continue;
        default:
          fprintf(stderr, "Non-critical error in function \"accept\": %s\nRepeat accept after pause\n", strerror(errno));
          sleep(1);
          continue;
      }
    }

    pid_t child_pid = fork();

    if (child_pid == 0)
    {
      close_descriptor(STDIN_FILENO);
      close_descriptor(STDOUT_FILENO);
      close_descriptor(server_socket);

      rc = process_connection(connection);

      close_descriptor(connection);
      break;
    }
    else if (child_pid > 0)
    {
      close_descriptor(connection);
    }
    else
    {
      fprintf(stderr, "Error in function \"fork\": %s\n", strerror(errno));
      close_descriptor(connection);
      rc = EXIT_FAILURE;
    }
  } while (rc == EXIT_SUCCESS);

  if (showboard_dir)
    free(showboard_dir);

  return rc;
}

void clean_up_child_process()
{
  int status;
  wait(&status);
}

int close_descriptor(int d)
{
  int rc, attempts = 0;
  do
  {
    rc = close(d);
    if (rc == -1)
    {
      switch (errno)
      {
        case EINTR:
          sleep(++attempts);
          continue;
        default:
          sleep(++attempts);
      }
    }
  }
  while (rc != 0 && attempts < MAX_CLOSE_DESCRIPTOR_ATTEMPTS);

  if (attempts == MAX_CLOSE_DESCRIPTOR_ATTEMPTS)
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

int process_connection(int conn)
{
  char buffer[STANDARD_BUFFER_SIZE];
  char* pos = buffer;
  ssize_t bytes_read = 0;
  int iError = NO_ERRORS;

  do
  {
    char* method = NULL;
    char* request = NULL;

    if (iError == NO_ERRORS)
      iError = skip_spaces(conn, buffer, sizeof(buffer), &pos, &bytes_read, 1000);

    while (iError == NO_ERRORS && !method)
    {
      method = read_word(conn, buffer, sizeof(buffer), &pos, &bytes_read, 4);
      if (!method && bytes_read == 0 && vbx_errno == LIMIT_EXCEEDED)
        iError = BAD_METHOD;
      else if (!method && bytes_read == 0)
        iError = vbx_errno;
      else if (!method)
      {
        while (bytes_read && (*pos == '\r' || *pos == '\n'))
        {
          bytes_read--;
          pos++;
        }
      }
      else if (method && !(strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0))
        iError = BAD_METHOD;
    }

    if (iError == NO_ERRORS)
      iError = skip_spaces(conn, buffer, sizeof(buffer), &pos, &bytes_read, 1000);

    while (iError == NO_ERRORS && !request)
    {
      request = read_word(conn, buffer, sizeof(buffer), &pos, &bytes_read, 32000);
      if (!request && bytes_read == 0 && vbx_errno == LIMIT_EXCEEDED)
        iError = BAD_REQUEST;
      else if (!request && bytes_read == 0)
        iError = vbx_errno;
      else if (!request)
      {
        while (bytes_read && (*pos == '\r' || *pos == '\n'))
        {
          bytes_read--;
          pos++;
        }
      }
    }

    if (iError == NO_ERRORS)
    {
      char* strline = NULL;
      ssize_t len = 0;
      int line_cnt = 0;

      do
      {
        strline = read_line(conn, buffer, sizeof(buffer), &pos, &bytes_read, 32000);
        if (!strline && bytes_read == 0)
          iError = vbx_errno;
        else if (!strline)
          iError = BAD_REQUEST;

        if (iError == NO_ERRORS)
        {
          line_cnt++;
          len = strlen(strline);
          if (len > 0)
            iError = parse_param_line(strline);
        }

        if (strline)
          free(strline);
      }
      while (iError == NO_ERRORS && line_cnt < HEAD_LINE_LIMIT && (len > 0 || line_cnt == 1) );

      if (iError == NO_ERRORS && line_cnt == HEAD_LINE_LIMIT)
        iError = LINE_LIMIT_EXCEEDED;
    }

    if (iError == NO_ERRORS)
      iError = process_get(conn, request);

    if (method)
      free(method);
    if (request)
      free(request);

    struct block_range *pbr = blocks;
    while (blocks)
    {
      pbr = blocks->pNext;
      free(blocks);
      blocks = pbr;
    }

  }
  while (iError == NO_ERRORS);

  switch (iError)
  {
    case NO_ERRORS:
      break;
    case CONNECTION_CLOSED:
      break;
    case TIME_OUT:
      send_request_timeout(conn);
      break;
    case BAD_REQUEST:
      send_bad_request(conn);
      break;
    case BAD_METHOD:
      send_bad_method(conn);
      break;
    case NOT_FOUND:
      showboard(conn);
      break;
  }

  if (iError == NO_ERRORS || iError == CONNECTION_CLOSED)
    return EXIT_SUCCESS;
  else
    return EXIT_FAILURE;
}

int process_get(int conn, const char* page)
{
  int rc = NO_ERRORS;
  char* v;

  if ((v = strstr(page, "?play=")))
    rc = player_show(conn, page);
  else if ((v = strstr(page, "?pump=")))
  {
    rc = pump(conn, page);
    if (rc != NO_ERRORS)
      rc = NO_ERRORS;
  }
  else
    rc = showboard(conn);

  return rc;
}

ssize_t read_block(const int conn, char* buf, const ssize_t buf_size)
{
  ssize_t bytes_read = 0;
  struct pollfd fds;
  fds.fd = conn;
  fds.events = POLLIN;
  fds.revents = 0;

  int rc = poll(&fds, 1, READ_BLOCK_TIME_LIMIT);
  if (rc == -1)
  {
    vbx_errno = POLL_ERROR;
    return -1;
  }
  else if (rc == 0)
  {
    vbx_errno = TIME_OUT;
    return -1;
  }
  else if (fds.revents & POLLHUP)
  {
    vbx_errno = CONNECTION_CLOSED;
    return -1;
  }
  else if (fds.revents & POLLERR)
  {
    vbx_errno = POLL_ERROR;
    return -1;
  }
  else if (fds.revents & POLLIN)
  {
    bytes_read = read(conn, buf, buf_size - 1);
    if (bytes_read == 0)
    {
      vbx_errno = CONNECTION_CLOSED;
      return -1;
    }
    else if (bytes_read == -1)
    {
      vbx_errno = READ_BLOCK_ERROR;
      return -1;
    }
  }

  if (bytes_read > 0)
    *(buf + bytes_read) = '\0';

  return bytes_read;
}

ssize_t write_block(const int conn, const char* buf, const ssize_t count)
{
  ssize_t bytes_wrote;
  struct pollfd fds;
  fds.fd = conn;
  fds.events = POLLOUT;
  fds.revents = 0;

  int rc = poll(&fds, 1, WRITE_BLOCK_TIME_LIMIT);
  if (rc == -1)
  {
    vbx_errno = POLL_ERROR;
    return -1;
  }
  else if (rc == 0)
  {
    vbx_errno = TIME_OUT;
    return -1;
  }
  else if (fds.revents & POLLHUP)
  {
    vbx_errno = CONNECTION_CLOSED;
    return -1;
  }
  else if (fds.revents & POLLERR)
  {
    vbx_errno = POLL_ERROR;
    return -1;
  }
  else if (fds.revents & POLLOUT)
  {
    bytes_wrote = write(conn, buf, count);
    if (bytes_wrote == 0)
    {
      vbx_errno = CONNECTION_CLOSED;
      return -1;
    }
    else if (bytes_wrote == -1)
    {
      vbx_errno = WRITE_BLOCK_ERROR;
      return -1;
    }
  }
  return bytes_wrote;
}

int skip_spaces(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t cnt, total_cnt = 0;

  do {
    cnt = 0;
    if (cnt == *bytes_read)
    {
      *pos = buf;
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read < 0)
      {
        *bytes_read = 0;
        return vbx_errno;
      }
    }

    while (cnt < *bytes_read && total_cnt + cnt < read_limit && **pos == ' ')
    {
      (*pos)++;
      cnt++;
    }

    total_cnt += cnt;

    if (cnt < *bytes_read && total_cnt == read_limit && **pos == ' ')
    {
      *pos = buf;
      *bytes_read = 0;
      vbx_errno = LIMIT_EXCEEDED;
      return BAD_REQUEST;
    }

    *bytes_read -= cnt;
  }
  while (*bytes_read == 0);

  return NO_ERRORS;
}

char* read_word(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  char* result = NULL;
  ssize_t word_len = 0, cnt;

  do {
    cnt = 0;
    if (cnt == *bytes_read)
    {
      *pos = buf;
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read < 0)
      {
        *bytes_read = 0;
        if (result)
          free(result);
        return NULL;
      }
    }

    while (cnt < *bytes_read && word_len + cnt < read_limit && **pos != ' ' && **pos != '\r' && **pos != '\n')
    {
      (*pos)++;
      cnt++;
    }

    word_len += cnt;

    if (cnt < *bytes_read && word_len == read_limit && **pos != ' ' && **pos != '\r' && **pos != '\n')
    {
      *pos = buf;
      *bytes_read = 0;
      vbx_errno = LIMIT_EXCEEDED;
      if (result)
        free(result);
      return NULL;
    }

    if (word_len > 0)
    {
      result = realloc(result, word_len + 1);
      if (!result)
      {
        vbx_errno = MALLOC_FAILED;
        return result;
      }
      memcpy(result + word_len - cnt, *pos - cnt, cnt);
    }
    *bytes_read -= cnt;
  }
  while (*bytes_read == 0);

  if (result)
    *(result + word_len) = '\0';
  return result;
}

char* read_line(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  char* result = NULL;
  ssize_t total_read = 0, cnt;

  do {
    cnt = 0;
    if (cnt == *bytes_read)
    {
      *pos = buf;
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read < 0)
      {
        *bytes_read = 0;
        if (result)
          free(result);
        return NULL;
      }
    }

    while (cnt < *bytes_read && total_read + cnt < read_limit && **pos != '\n')
    {
      (*pos)++;
      cnt++;
    }

    total_read += cnt;

    if (cnt < *bytes_read && total_read == read_limit && **pos != '\n')
    {
      *pos = buf;
      *bytes_read = 0;
      vbx_errno = LIMIT_EXCEEDED;
      if (result)
        free(result);
      return NULL;
    }

    result = realloc(result, total_read + 1);
    if (!result)
    {
      vbx_errno = MALLOC_FAILED;
      return result;
    }
    memcpy(result + total_read - cnt, *pos - cnt, cnt);
    *bytes_read -= cnt;
  }
  while (*bytes_read == 0);

  (*pos)++;
  (*bytes_read)--;

  if (result)
  {
    if (result[total_read-1] == '\r')
      total_read--;
    *(result + total_read) = '\0';
  }
  return result;
}

int parse_param_line(char* str)
{
  int rc;
  char* saveptr;

  while (*str == ' ') str++;
  saveptr = str;
  str = find_str_ncase(str, "Range:");
  if (str && saveptr == str)
  {
    str += 6;
    while (*str == ' ') str++;
    saveptr = str;
    str = find_str_ncase(str, "bytes=");
    if (str && saveptr == str)
    {
      str += 6;
      while (*str == ' ') str++;

      long start_pos;
      long end_pos;
      struct block_range *pbr, *tail;

      char* token = strtok_r(str, ",", &saveptr);
      while (token)
      {
        start_pos = end_pos = 0;

        if ( strspn(token, "0123456789- ") == strlen(token) )
        {
          rc = sscanf(token, "%ld - %ld", &start_pos, &end_pos);
          if (rc <= 0)
            start_pos = end_pos = 0;

          if ( !(start_pos == 0 && end_pos == 0) && !(start_pos < 0 && end_pos != 0) && (start_pos < end_pos || end_pos == 0) )
          {
            pbr = malloc(sizeof(struct block_range));
            if (pbr)
            {
              pbr->start = start_pos;
              pbr->end = end_pos;
              pbr->pNext = NULL;
              if (!blocks)
                blocks = tail = pbr;
              else
              {
                tail->pNext = pbr;
                tail = pbr;
              }
            }
            else
              return MALLOC_FAILED;
          }
        }

        token = strtok_r(NULL, ",", &saveptr);
      }
    }
  }

  return NO_ERRORS;
}

char* find_str_ncase(const char* buf, const char* str)
{
  int mn = 'a' > 'A' ? 'A' : 'a';
  int dist = 'a' < 'A' ? 'A' - 'a' : 'a' - 'A';
  char* pos = (char*)str;
  char c1, c2;

  while (*buf != '\0' && *pos != '\0')
  {
    c1 = *buf;
    if (c1 >= mn && c1 < mn + dist)
      c1 += dist;
    c2 = *pos;
    if (c2 >= mn && c2 < mn + dist)
      c2 += dist;

    if ( c1 == c2)
      pos++;
    else
    {
      buf -= pos - str;
      pos = (char*)str;
    }
    buf++;
  }

  if (*pos == '\0')
  {
    buf -= pos - str;
    return (char*)buf;
  }
  return NULL;
}

void send_bad_request(const int conn)
{
  char* bad_request_response =
    "HTTP/1.0 400 Bad Reguest\n"
    "Content-type: text/html\n"
    "\n";

  ssize_t lenght = strlen(bad_request_response);
  write_block(conn, bad_request_response, lenght);
}

void send_request_timeout(const int conn)
{
  char* timeout_response =
    "HTTP/1.1 408 Request Timeout\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Request Timeout</h1>\n"
    "  <p>Your requests are coming in too slowly. The server is forced to reject connection.</p>\n"
    " </body>\n"
    "</html>\n"
    "\n";

  ssize_t lenght = strlen(timeout_response);
  write_block(conn, timeout_response, lenght);
}

void send_bad_method(const int conn)
{
  char* bad_method_response =
    "HTTP/1.0 501 Method Not Implemented\n"
    "Content-type: text/html\n"
    "\n";

  ssize_t lenght = strlen(bad_method_response);
  write_block(conn, bad_method_response, lenght);
}
