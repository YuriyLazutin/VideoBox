#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // readlink
#include <errno.h> // errno
#include <string.h> // strcpy, strerror
#include <arpa/inet.h>
#include <sys/wait.h>
//#include <sys/socket.h>
#include <signal.h>
//#include <linux/limits.h> // PATH_MAX
#include <poll.h>
#include "defines.h"
#include "player.h"
#include "pump.h"
#include "showboard.h"

#ifndef NDEBUG
  #include "logger.h"
  int connection_id;
  void read_tail(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
#endif

// catch SIGCHLD
void clean_up_child_process(int);
int close_descriptor(int d);
int process_connection(int conn);
int process_get(int conn, const char* page);
ssize_t read_block(const int conn, char* buf, const ssize_t buf_size);
ssize_t write_block(const int conn, const char* buf, const ssize_t count);
int skip_spaces(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
char* read_word(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
char* read_line(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
int read_str(const int conn, char* str, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
void send_bad_request(const int conn);
void send_request_timeout(const int conn);
void send_bad_method(const int conn);
char* showboard_dir;
ssize_t showboard_dir_length;

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

  //free(showboard_dir);  // Just for debug
  //showboard_dir = strdup("/home/ylazutin/showboard/"); // Just for debug
  //showboard_dir_length = strlen(showboard_dir); // Just for debug

  #ifndef NDEBUG
    server_log_init();
    connection_id = 0;
  #endif

  struct sockaddr_in server_address;
  struct sockaddr_in remote_address;
  socklen_t address_length = sizeof(server_address);
  memset(&server_address, 0, address_length);
  server_address.sin_family = AF_INET;
  server_address.sin_port = (uint16_t)htons(SERVER_PORT);
  struct in_addr local_address;
  local_address.s_addr = INADDR_ANY;
  server_address.sin_addr = local_address;

  // Set up SIGCHLD handler
  struct sigaction sigchld_action;
  memset(&sigchld_action, 0, sizeof(sigchld_action));
  sigchld_action.sa_handler = &clean_up_child_process;
  sigaction(SIGCHLD, &sigchld_action, NULL);

  // Server creation
  int server_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (server_socket == -1)
  {
    #ifndef NDEBUG
      log_print("Error in function \"socket\": %s\n", strerror(errno));
    #else
    fprintf(stderr, "Error in function \"socket\": %s\n", strerror(errno));
    #endif
    return EXIT_FAILURE;
  }

  const int on = 1;
  int rc = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  #ifndef NDEBUG
  if (rc == -1)
    log_print("Error in function \"setsockopt\": %s\n", strerror(errno));
  #endif

  rc = bind(server_socket, (const struct sockaddr *)&server_address, sizeof(server_address));
  if (rc != 0)
  {
    #ifndef NDEBUG
      log_print("Error in function \"bind\": %s\n", strerror(errno));
    #else
    fprintf(stderr, "Error in function \"bind\": %s\n", strerror(errno));
    #endif
    return EXIT_FAILURE;
  }

  rc = listen(server_socket, SERVER_LISTEN_BACKLOG);
  if (rc != 0)
  {
    #ifndef NDEBUG
      log_print("Error in function \"listen\": %s\n", strerror(errno));
    #else
    fprintf(stderr, "Error in function \"listen\": %s\n", strerror(errno));
    #endif
    return EXIT_FAILURE;
  }

  // Process connections
  do
  {
    address_length = sizeof(remote_address);
    memset(&remote_address, 0, address_length);
    int connection = accept(server_socket, (struct sockaddr *)&remote_address, &address_length);
    if (connection == -1)
    {
      switch (errno)
      {
        case EINTR:               // The system call was interrupted by a signal.
          continue;

        // Critical errors (shutdown server)
        case EBADF:             // invalid descriptor. server_socket is not an open file descriptor.
        case EINVAL:            // Socket is not listening for connections, or address_length is invalid
        case ENOTSOCK:          // The file descriptor server_socket does not refer to a socket.
        case EOPNOTSUPP:        // The referenced socket is not of type SOCK_STREAM.
        /*
          // In addition, network errors for the new socket and as defined for the protocol may be returned.
          case ERESTART:          // Interrupted system call should be restarted (The value ERESTARTSYS may be seen during a trace).
          case ESOCKTNOSUPPORT:   // Socket type not supported
          case EPROTONOSUPPORT:   // Protocol not supported
        */
          #ifndef NDEBUG
            log_print("Critical error in function \"accept\": %s\n", strerror(errno));
          #else
            fprintf(stderr, "Critical error in function \"accept\": %s\n", strerror(errno));
          #endif
          rc = EXIT_FAILURE;
          continue;

        default:  // Not critical errors (Repeat attempt after pause)
        /*
          EAGAIN, EWOULDBLOCK:   The socket is marked nonblocking and no connections are present to be accepted.
          ECONNABORTED:          A connection has been aborted.
          EFAULT:                The remote_address argument is not in a writable part of the user address space.
          EMFILE:                The per-process limit on the number of open file descriptors has been reached.
          ENFILE:                The system-wide limit on the total number of open files has been reached.
          ENOBUFS, ENOMEM:       Not enough free memory.  This often means that the memory allocation is limited by the socket buffer limits, not by the system memory.
          EPERM:                 Firewall rules forbid connection.
          EPROTO:                Protocol error.

          // In addition, network errors for the new socket and as defined for the protocol may be returned. Various Linux kernels can return other errors
          ENOSR:                 Out of streams resources
          ETIMEDOUT:             Connection timed out
        */
          #ifndef NDEBUG
            log_print("Non-critical error in function \"accept\": %s\nRepeat accept after pause\n", strerror(errno));
          #else
            fprintf(stderr, "Non-critical error in function \"accept\": %s\nRepeat accept after pause\n", strerror(errno));
          #endif
          sleep(1);
          continue;

      }
    }
    #ifndef NDEBUG
    else
    {
      connection_id++;
      log_print("Connection accepted from %s:%d (connection_id = %d)\n", inet_ntoa(remote_address.sin_addr), (int)ntohs(remote_address.sin_port), connection_id);
    }
    #endif

    pid_t child_pid = fork();

    if (child_pid == 0) // Child
    {
      // Close descriptors stdin, stdout, server_socket
      close_descriptor(STDIN_FILENO);
      close_descriptor(STDOUT_FILENO);
      close_descriptor(server_socket);
      #ifndef NDEBUG
        // Close server log descriptor and open connection log descriptor
        log_close();
        connection_log_init(connection_id);
      #endif

      rc = process_connection(connection);

      close_descriptor(connection);
      break;
    }
    else if (child_pid > 0) // Parent
    {
      close_descriptor(connection);
    }
    else // fork() failed
    {
      #ifndef NDEBUG
        log_print("Error in function \"fork\": %s\n", strerror(errno));
      #else
      fprintf(stderr, "Error in function \"fork\": %s\n", strerror(errno));
      #endif
      close_descriptor(connection);
      rc = EXIT_FAILURE;
    }
  } while (rc == EXIT_SUCCESS);

  if (showboard_dir)
    free(showboard_dir);

  #ifndef NDEBUG
    log_close();
  #endif

  return rc;
}

// catch SIGCHLD
void clean_up_child_process(int signal_number)
{
  #ifndef NDEBUG
    log_print("Received signal: %d\n", signal_number);
  #endif
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
        case EINTR:               // The system call was interrupted by a signal.
          sleep(++attempts);
          continue;
        default:
        /*
          EBADF:                 fd isn't a valid open file descriptor.
          EIO:                   An I/O error occurred.
          ENOSPC, EDQUOT:        On NFS, these errors are not normally reported against the first write which exceeds the available storage space,
                                 but instead against a subsequent write(), fsync(), or close().
        */
          #ifndef NDEBUG
            log_print("Error in function \"close_descriptor(%d)\": %s\nRepeat close(descriptor) after pause\n", d, strerror(errno));
          #endif
          sleep(++attempts);
      }
    }
  }
  while (rc != 0 && attempts < MAX_CLOSE_DESCRIPTOR_ATTEMPTS);

  if (attempts == MAX_CLOSE_DESCRIPTOR_ATTEMPTS)
  {
    #ifndef NDEBUG
      log_print("Tried to close_descriptor(%d), but all attempts failed.\n", d);
    #endif
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int process_connection(int conn)
{
  #ifndef NDEBUG
  char buffer[TINY_BUFFER_SIZE];
  //char buffer[SMALL_BUFFER_SIZE];
  #else
  char buffer[STANDARD_BUFFER_SIZE];
  #endif
  char* pos = buffer;
  ssize_t bytes_read;

  char* method = NULL;
  char* request = NULL;
  int iError = NO_ERRORS;

  #ifndef NDEBUG
    log_print("Received from client\n");
  #endif
  bytes_read = read_block(conn, buffer, sizeof(buffer));
  if (bytes_read == -1 * TIME_OUT)
    iError = TIME_OUT;
  else if (bytes_read < 0)
    iError = BAD_REQUEST;
  else if (bytes_read == 0)
    iError = CONNECTION_CLOSED;

  if (iError == NO_ERRORS)
    iError = skip_spaces(conn, buffer, sizeof(buffer), &pos, &bytes_read, 1000);

  if (iError == NO_ERRORS)
  {
    method = read_word(conn, buffer, sizeof(buffer), &pos, &bytes_read, 5);
    if (!method && bytes_read == 0)
      iError = CONNECTION_CLOSED;
  }

  if (iError == NO_ERRORS)
  {
    if ( !method || !(strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) )
      iError = BAD_METHOD;
  }

  if (iError == NO_ERRORS)
    iError = skip_spaces(conn, buffer, sizeof(buffer), &pos, &bytes_read, 1000);

  if (iError == NO_ERRORS)
  {
    request = read_word(conn, buffer, sizeof(buffer), &pos, &bytes_read, 32000);
    if (!request && bytes_read == 0)
      iError = CONNECTION_CLOSED;
  }

  if (iError == NO_ERRORS)
  {
    if (!request)
      iError = BAD_REQUEST;
  }

  if (iError == NO_ERRORS)
  {
    iError = read_str(conn, "\r\n\r\n", buffer, sizeof(buffer), &pos, &bytes_read, 32000);
    int slen = strlen("\r\n\r\n");
    pos += slen;
    bytes_read -= slen;
  }

  if (iError == NO_ERRORS)
    iError = process_get(conn, request);

  #ifndef NDEBUG
  if (iError == NO_ERRORS)
    read_tail(conn, buffer, sizeof(buffer), &pos, &bytes_read, 32000);
  #endif

  if (method)
    free(method);
  if (request)
    free(request);

  switch (iError)
  {
    case NO_ERRORS:
      #ifndef NDEBUG
        log_print("Request successfully completed.\n");
      #endif
      break;
    case CONNECTION_CLOSED:
      #ifndef NDEBUG
        log_print("Connection unexpectedly terminated.\n");
      #endif
      break;
    case TIME_OUT:
      send_request_timeout(conn);
      #ifndef NDEBUG
        log_print("Timeout exceeded.\n");
      #endif
      break;
    case BAD_REQUEST:
      send_bad_request(conn);
      #ifndef NDEBUG
        log_print("Bad request received. Request completed.\n");
      #endif
      break;
    case BAD_METHOD:
      send_bad_method(conn);
      #ifndef NDEBUG
        log_print("Bad method received. Request completed.\n");
      #endif
      break;
    case NOT_FOUND:
      showboard(conn);
      #ifndef NDEBUG
        log_print("Resource not found. Request completed.\n");
      #endif
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
    rc = pump(conn, page);
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
    #ifndef NDEBUG
      log_print("Poll error: %s\n", strerror(errno));
    #endif
  }
  else if (rc == 0)
  {
    #ifndef NDEBUG
      log_print("Poll error: Timeout exceeded\n");
    #endif
    return -1 * TIME_OUT;
  }
  else if (fds.revents & POLLHUP)
  {
    #ifndef NDEBUG
      log_print("Poll info: Connection was terminated\n");
    #endif
    return -1 * CONNECTION_CLOSED;
  }
  else if (fds.revents & POLLERR)
  {
    #ifndef NDEBUG
      log_print("Poll info: POLLERR\n");
    #endif
    return 0; // ?
  }
  else if (fds.revents & POLLIN)
  {
    bytes_read = read(conn, buf, buf_size - 1);
    if (bytes_read > 0)
    {
      *(buf + bytes_read) = '\0';
      #ifndef NDEBUG
        log_print("%s", buf);
      #endif
    }
    else if (bytes_read == 0)
    {
      #ifndef NDEBUG
        log_print("Connection terminated\n");
      #endif
    }
    else // (bytes_read == -1)
    {
      #ifndef NDEBUG
        log_print("Error in function \"read\": %s\n", strerror(errno));
        log_print("Connection unexpectedly terminated\n");
      #endif
    }
  }
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
    #ifndef NDEBUG
      log_print("write_block->Poll error: %s\n", strerror(errno));
    #endif
  }
  else if (rc == 0)
  {
    #ifndef NDEBUG
      log_print("write_block->Poll error: Timeout exceeded\n");
    #endif
    return -1 * TIME_OUT;
  }
  else if (fds.revents & POLLHUP)
  {
    #ifndef NDEBUG
      log_print("write_block->Poll info: Connection was terminated\n");
    #endif
    return -1 * CONNECTION_CLOSED;
  }
  else if (fds.revents & POLLERR)
  {
    #ifndef NDEBUG
      log_print("write_block->Poll info: POLLERR\n");
    #endif
    return 0; // ?
  }
  else if (fds.revents & POLLOUT)
  {
    bytes_wrote = write(conn, buf, count);
    if (bytes_wrote > 0)
    {
      //*(buf + bytes_read) = '\0';
      //#ifndef NDEBUG
      //  log_print("%s", buf);
      //#endif
    }
    else if (bytes_wrote == 0)
    {
      #ifndef NDEBUG
        log_print("write_block->Connection terminated\n");
        return -1 * CONNECTION_CLOSED;
      #endif
    }
    else // (bytes_wrote == -1)
    {
      #ifndef NDEBUG
        log_print("write_block->Error in function \"write\": %s\n", strerror(errno));
        log_print("write_block->Connection unexpectedly terminated\n");
      #endif
      return -1 * WRITE_BLOCK_ERROR;
    }
  }
  return bytes_wrote;
}

int skip_spaces(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t cnt, total_cnt = 0;

  while (**pos == ' ')
  {
    cnt = 0;
    do
    {
      (*pos)++;
      cnt++;
    }
    while (cnt < *bytes_read && **pos == ' ');

    total_cnt += cnt;

    if (total_cnt > read_limit)
    {
      #ifndef NDEBUG
      log_print("Limit exceeded while skip spaces\n");
      #endif
      *bytes_read -= cnt;
      return BAD_REQUEST;
    }

    if (cnt == *bytes_read)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read == -1 * TIME_OUT)
      {
        #ifndef NDEBUG
          log_print("Time out while skip spaces\n");
        #endif
        return TIME_OUT;
      }
      else if (*bytes_read < 0)
      {
        #ifndef NDEBUG
          log_print("Read block error while skip spaces\n");
        #endif
        return READ_BLOCK_ERROR;
      }
      else if (*bytes_read == 0)
      {
        #ifndef NDEBUG
          log_print("Point 1: received 0 bytes\n");
        #endif
        return CONNECTION_CLOSED;
      }
      *pos = buf;
    }
    else
      *bytes_read -= cnt;
  }
  return NO_ERRORS;
}

char* read_word(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  char* result = NULL;
  ssize_t word_len = 0, cnt;

  while (**pos != ' ' && **pos != '\r' && **pos != '\n')
  {
    cnt = 0;
    do
    {
      (*pos)++;
      cnt++;
    }
    while (cnt < *bytes_read && **pos != ' ' && **pos != '\r' && **pos != '\n');

    word_len += cnt;

    if (word_len + 1 > read_limit)
    {
      if (result)
        free(result);
      #ifndef NDEBUG
      log_print("Limit exceeded while read_word\n");
      #endif
      *bytes_read -= cnt;
      return NULL;
    }

    result = realloc(result, word_len + 1);
    if (!result)
    {
      #ifndef NDEBUG
        log_print("Realloc filed while read_word\n");
      #endif
      return NULL;
    }
    memcpy(result + word_len - cnt, *pos - cnt, cnt);

    if (cnt == *bytes_read)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read == -1 * TIME_OUT)
      {
        #ifndef NDEBUG
          log_print("Time out while read word\n");
        #endif
        if (result)
          free(result);
        return NULL;
      }
      else if (*bytes_read < 0)
      {
        #ifndef NDEBUG
          log_print("Read block error while read word\n");
        #endif
        if (result)
          free(result);
        return NULL;
      }
      else if (*bytes_read == 0)
      {
        #ifndef NDEBUG
          log_print("Point 2: received 0 bytes\n");
        #endif
        if (result)
          free(result);
        break;
      }
      *pos = buf;
    }
    else
      *bytes_read -= cnt;
  }

  if (result)
    *(result + word_len) = '\0';
  return result;
}

char* read_line(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  char* result = NULL;
  ssize_t total_read = 0, cnt;

  while (**pos != '\n')
  {
    cnt = 0;
    do
    {
      (*pos)++;
      cnt++;
    }
    while (cnt < *bytes_read && **pos != '\n');

    total_read += cnt;

    if (total_read + 1 > read_limit)
    {
      if (result)
        free(result);
      #ifndef NDEBUG
      log_print("Limit exceeded while read_line\n");
      #endif
      *bytes_read -= cnt;
      return NULL;
    }

    result = realloc(result, total_read + 1);
    if (!result)
    {
      #ifndef NDEBUG
        log_print("Realloc filed while read_line\n");
      #endif
      return NULL;
    }
    memcpy(result + total_read - cnt, *pos - cnt, cnt);

    if (cnt == *bytes_read)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read == -1 * TIME_OUT)
      {
        #ifndef NDEBUG
          log_print("Time out while read_line\n");
        #endif
        if (result)
          free(result);
        return NULL;
      }
      else if (*bytes_read < 0)
      {
        #ifndef NDEBUG
          log_print("Read block error while read_line\n");
        #endif
        if (result)
          free(result);
        return NULL;
      }
      else if (*bytes_read == 0)
      {
        #ifndef NDEBUG
          log_print("read_line: received 0 bytes\n");
        #endif
        if (result)
          free(result);
        break;
      }
      *pos = buf;
    }
    else
      *bytes_read -= cnt;
  }

  if (result)
  {
    if (result[total_read] == '\r')
      total_read--;
    *(result + total_read) = '\0';
  }
  return result;
}

int read_str(const int conn, char* str, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t total_read = *bytes_read;
  int i = 0, n = strlen(str), m;
  char* pstr;

  if (n > buf_size - 1)
    return BAD_REQUEST;

  while ((pstr = strstr(*pos, str)) == NULL)
  {
    if (total_read > read_limit)
    {
      #ifndef NDEBUG
        log_print("Limit exceeded while reading string\n");
      #endif
      *bytes_read = 0;
      return BAD_REQUEST;
    }

    m = (*bytes_read < n) ? *bytes_read : n;

    for (i = 0; i < m; i++)
      *(buf + i) = *(*pos + *bytes_read - m + i);
    *pos = buf + i;

    *bytes_read = read_block(conn, *pos, buf_size - i);
    if (*bytes_read == -1 * TIME_OUT)
      {
        #ifndef NDEBUG
          log_print("Time out while read str\n");
        #endif
        return TIME_OUT;
      }
      else if (*bytes_read < 0)
      {
        #ifndef NDEBUG
          log_print("Read block error while read str\n");
        #endif
        return READ_BLOCK_ERROR;
      }
      else if (*bytes_read == 0)
      {
       #ifndef NDEBUG
          log_print("Point 3: received 0 bytes\n");
        #endif
        return CONNECTION_CLOSED;
      }

    total_read += *bytes_read;
    *pos = buf;
    *bytes_read += m;
  }

  *bytes_read = *bytes_read - (pstr - *pos);
  *pos = pstr;
  return NO_ERRORS;
}

#ifndef NDEBUG
void read_tail(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  int flags = fcntl(conn, F_GETFL);
  if (flags == -1)
  {
    log_print("read_tail: Error in call fcntl(conn, F_GETFL): %s\n", strerror(errno));
    return;
  }

  int rc = fcntl(conn, F_SETFL, flags | O_NONBLOCK);
  if (rc == -1)
  {
    log_print("read_tail: Error in call fcntl(conn, F_SETFL): %s\n", strerror(errno));
    return;
  }

  // Just for test
  int newflags = fcntl(conn, F_GETFL);
  if (newflags == -1)
    fprintf(stdout, "read_tail: Error 2 in call fcntl(conn, F_GETFL): %s\n", strerror(errno));
  else if (newflags & O_NONBLOCK)
    fprintf(stdout, "read_tail: Non-block mode is on.\n");
  // End of Just for test

  ssize_t cnt = 0;

  do
  {
    if (*bytes_read <= 0)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read == -1 * TIME_OUT)
      {
        log_print("Time out while read tail\n");
        return;
      }
      else if (*bytes_read < 0)
      {
        log_print("Read block error while read tail\n");
        return;
      }
      else if (*bytes_read == 0)
      {
        log_print("Point 4: received 0 bytes\n");
        return;
      }
      *pos = buf;
    }

    if (*bytes_read > 0)
    {
      if (cnt == 0)
        log_print("Received tail from client:\n");

      log_print("%s", *pos);
      cnt += *bytes_read;

      if (cnt > read_limit)
      {
        log_print("Limit exceeded while read_tail\n");
        break;
      }
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read == -1 * TIME_OUT)
      {
        log_print("Time out while read tail\n");
        return;
      }
      else if (*bytes_read < 0)
      {
        log_print("Read block error while read tail\n");
        return;
      }
      else if (*bytes_read == 0)
      {
        log_print("Point 4: received 0 bytes\n");
        return;
      }
      *pos = buf;
    }
  }
  while (*bytes_read);

  rc = fcntl(conn, F_SETFL, flags);
  if (rc == -1)
  {
    log_print("read_tail: Error 2 in call fcntl(conn, F_SETFL): %s\n", strerror(errno));
    return;
  }

  // Just for test
  newflags = fcntl(conn, F_GETFL);
  if (newflags == -1)
    fprintf(stdout, "read_tail: Error 3 in call fcntl(conn, F_GETFL): %s\n", strerror(errno));
  else if (newflags != flags)
    fprintf(stdout, "read_tail: Error! Flags was not restored.\n");
  // End of Just for test
}
#endif

void send_bad_request(const int conn)
{
  char* bad_request_response =
    "HTTP/1.0 400 Bad Reguest\n"
    "Content-type: text/html\n"
    "\n";

  write(conn, bad_request_response, strlen(bad_request_response));
  #ifndef NDEBUG
    log_print("Sended to client:\n");
    log_print("%s", bad_request_response);
  #endif
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
    "</html>\n";

  write(conn, timeout_response, strlen(timeout_response));
  #ifndef NDEBUG
    log_print("Sended to client:\n");
    log_print("%s", timeout_response);
  #endif
}

void send_bad_method(const int conn)
{
  char* bad_method_response =
    "HTTP/1.0 501 Method Not Implemented\n"
    "Content-type: text/html\n"
    "\n";

  write(conn, bad_method_response, strlen(bad_method_response));
  #ifndef NDEBUG
    log_print("Sended to client:\n");
    log_print("%s", bad_method_response);
  #endif
}
