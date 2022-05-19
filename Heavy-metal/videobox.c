#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
//#include <linux/limits.h> // PATH_MAX
#include "defines.h"
#include "player.h"
#include "pump.h"
#include "showboard.h"

// catch SIGCHLD
void clean_up_child_process(int);
int process_connection(int conn);
int process_get(int conn, const char* page);
ssize_t read_block(const int conn, char* buf, const ssize_t buf_size);
int skip_spaces(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
char* read_word(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
int read_str(const int conn, char* str, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
void send_bad_request(const int conn);
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
  while (showboard_dir_length > 0 && showboard_dir[showboard_dir_length - 1] != '/')
    showboard_dir_length--;
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
    fprintf(stderr, "Error in function \"socket\": %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  int rc = bind(server_socket, (const struct sockaddr *)&server_address, sizeof(server_address));
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

  // Process connections
  do
  {
    address_length = sizeof(remote_address);
    memset(&remote_address, 0, address_length);
    int connection = accept(server_socket, (struct sockaddr *)&remote_address, &address_length);
    if (connection == -1)
    {
      int errcode = errno;
      fprintf(stderr, "Error in function \"accept\": %s\n", strerror(errcode));

      switch (errcode)
      {
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
          rc = EXIT_FAILURE;
          continue;

        default:  // Not critical errors (Repeat attempt after pause)
        /*
          EAGAIN, EWOULDBLOCK:   The socket is marked nonblocking and no connections are present to be accepted.
          ECONNABORTED:          A connection has been aborted.
          EFAULT:                The remote_address argument is not in a writable part of the user address space.
          EINTR:                 The system call was interrupted by a signal.
          EMFILE:                The per-process limit on the number of open file descriptors has been reached.
          ENFILE:                The system-wide limit on the total number of open files has been reached.
          ENOBUFS, ENOMEM:       Not enough free memory.  This often means that the memory allocation is limited by the socket buffer limits, not by the system memory.
          EPERM:                 Firewall rules forbid connection.
          EPROTO:                Protocol error.

          // In addition, network errors for the new socket and as defined for the protocol may be returned. Various Linux kernels can return other errors
          ENOSR:                 Out of streams resources
          ETIMEDOUT:             Connection timed out
        */
          sleep(1);
          continue;

      }
    }
    else
      fprintf(stdout, "Connection accepted from %s:%d\n", inet_ntoa(remote_address.sin_addr), (int)ntohs(remote_address.sin_port));

    pid_t child_pid = fork();

    if (child_pid == 0) // Child
    {
      // Close descriptors stdin, stdout, server_socket
      close(STDIN_FILENO);
      //close(STDOUT_FILENO);
      close(server_socket);

      rc = process_connection(connection);

      close(connection);
      return rc;
    }
    else if (child_pid > 0) // Parent
    {
      close(connection);
    }
    else // fork() failed
    {
      fprintf(stderr, "Error in function \"fork\": %s\n", strerror(errno));
      rc = EXIT_FAILURE;
    }
  } while (rc == EXIT_SUCCESS);

  if (showboard_dir)
    free(showboard_dir);
  return rc;
}

// catch SIGCHLD
void clean_up_child_process(int signal_number)
{
  fprintf(stderr, "Received signal: %d", signal_number);
  int status;
  wait(&status);
}

int process_connection(int conn)
{
  char buffer[SMALL_BUFFER_SIZE];
  char* pos = buffer;
  ssize_t bytes_read;

  char* method = NULL;
  char* request = NULL;
  int iError = NO_ERRORS;

  fprintf(stdout, "Connection %d: Received from client\n", conn);
  bytes_read = read_block(conn, buffer, sizeof(buffer));
  if (bytes_read <= 0)
    iError = BAD_REQUEST;

  if (iError == NO_ERRORS)
    iError = skip_spaces(conn, buffer, sizeof(buffer), &pos, &bytes_read, 1000);

  if (iError == NO_ERRORS)
  {
    method = read_word(conn, buffer, sizeof(buffer), &pos, &bytes_read, 5);

    if ( !method || !(strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0) )
      iError = BAD_METHOD;
  }

  if (iError == NO_ERRORS)
    iError = skip_spaces(conn, buffer, sizeof(buffer), &pos, &bytes_read, 1000);

  if (iError == NO_ERRORS)
  {
    request = read_word(conn, buffer, sizeof(buffer), &pos, &bytes_read, 32000);
    if (!request)
      iError = BAD_REQUEST;
  }
  if (iError == NO_ERRORS)
    iError = read_str(conn, "\r\n\r\n", buffer, sizeof(buffer), &pos, &bytes_read, 32000);

  if (iError == NO_ERRORS)
    process_get(conn, request);

  if (method)
    free(method);
  if (request)
    free(request);

  switch (iError)
  {
    case NO_ERRORS:
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

  if (iError == NO_ERRORS)
    return EXIT_SUCCESS;
  else
    return EXIT_FAILURE;
}

int process_get(int conn, const char* page)
{
  int rc = EXIT_SUCCESS;
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
  ssize_t bytes_read;
  bytes_read = read(conn, buf, buf_size - 1);
  if (bytes_read > 0)
  {
    *(buf + bytes_read) = '\0';
    fprintf(stdout, "%s", buf);
  }
  else if (bytes_read == 0)
    fprintf(stdout, "Connection %d terminated without sending any data\n", conn);
  else // (bytes_read == -1)
  {
    fprintf(stderr, "Error in function \"read\": %s\n", strerror(errno));
    fprintf(stderr, "Connection %d unexpectedly terminated\n", conn);
  }
  return bytes_read;
}

int skip_spaces(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t cnt, total_cnt = 0;

  while (**pos == ' ')
  {
    cnt = 1;

    while (*(++*pos) == ' ' && ++cnt < *bytes_read);

    total_cnt += cnt;

    if (total_cnt > read_limit)
    {
      fprintf(stderr, "Connection %d: limit exceeded while skip spaces\n", conn);
      *bytes_read -= cnt;
      return BAD_REQUEST;
    }

    if (cnt == *bytes_read)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read == 0)
        fprintf(stderr, "Point 1: received 0 bytes\n");
        //return CONNECTION_CLOSED;
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
    cnt = 1;
    do (*pos)++;
    while (**pos != ' ' && **pos != '\r' && **pos != '\n' && ++cnt < *bytes_read);

    word_len += cnt;

    if (word_len + 1 > read_limit)
    {
      if (result)
        free(result);
      fprintf(stderr, "Connection %d: limit exceeded while read_word\n", conn);
      *bytes_read -= cnt;
      return NULL;
    }

    result = realloc(result, word_len + 1);
    memcpy(result + word_len - cnt, *pos - cnt, cnt);

    if (cnt == *bytes_read)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      if (*bytes_read == 0)
        fprintf(stderr, "Point 2: received 0 bytes\n");
      //return CONNECTION_CLOSED;

      *pos = buf;
    }
    else
      *bytes_read -= cnt;
  }

  if (result)
    *(result + word_len) = '\0';
  return result;
}

int read_str(const int conn, char* str, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t total_read = *bytes_read;
  int i = 0, n = strlen(str);
  char* pstr;

  if (n > buf_size - 1)
    return BAD_REQUEST;

  while ((pstr = strstr(*pos, str)) == NULL)
  {
    if (total_read > read_limit)
    {
      fprintf(stderr, "Connection %d: limit exceeded while reading string\n", conn);
      *bytes_read = 0;
      return BAD_REQUEST;
    }

    if (*bytes_read < n)
      n = *bytes_read;
    for (i = 0; i < n; i++)
      *(buf + i) = *(*pos + *bytes_read - n + i);
    *pos = buf + i;

    *bytes_read = read_block(conn, *pos, buf_size - i);
    if (*bytes_read == 0)
      fprintf(stderr, "Point 3: received 0 bytes\n");
    //return CONNECTION_CLOSED;

    total_read += *bytes_read;
    *pos = buf;
  }

  *bytes_read = *bytes_read + i - (pstr - *pos);
  *pos = pstr;
  return NO_ERRORS;
}

void send_bad_request(const int conn)
{
  char* bad_request_response =
    "HTTP/1.0 400 Bad Reguest\n"
    "Content-type: text/html\n"
    "\n";

  write(conn, bad_request_response, strlen(bad_request_response));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", bad_request_response);
}

void send_bad_method(const int conn)
{
  char* bad_method_response =
    "HTTP/1.0 501 Method Not Implemented\n"
    "Content-type: text/html\n"
    "\n";

  write(conn, bad_method_response, strlen(bad_method_response));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", bad_method_response);
}
