#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <linux/limits.h>
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
int read_rnrn(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit);
void send_ok(const int conn);
void send_bad_request(const int conn);
void send_bad_method(const int conn);
int service_detect(const char* service_token, const char** pos);
char* server_dir;
char* get_self_executable_directory();

int main(int argc, char** argv)
{
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

  server_dir = get_self_executable_directory();

  // Process connections
  while (1)
  {
    address_length = sizeof(remote_address);
    memset(&remote_address, 0, address_length);
    int connection = accept(server_socket, (struct sockaddr *)&remote_address, &address_length);
    if (connection == -1)
    {
      if (errno == EINTR) // Function was interrupded by signal. Repeat attempt
        continue;
      else // Something wrong
      {
        fprintf(stderr, "Error in function \"accept\": %s\n", strerror(errno));
        if (server_dir)
          free(server_dir);
        return EXIT_FAILURE;
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
      if (server_dir)
        free(server_dir);
      return EXIT_FAILURE;
    }
  }

  if (server_dir)
    free(server_dir);
  return EXIT_SUCCESS;
}

// catch SIGCHLD
void clean_up_child_process(int signal_number)
{
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
  char* protocol = NULL;
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
    iError = skip_spaces(conn, buffer, sizeof(buffer), &pos, &bytes_read, 1000);

  if (iError == NO_ERRORS)
  {
    protocol = read_word(conn, buffer, sizeof(buffer), &pos, &bytes_read, 9);
    if ( !protocol || !(strcmp(protocol, "HTTP/1.0") == 0 || strcmp(protocol, "HTTP/1.1") == 0) )
      iError = BAD_REQUEST;
  }

  if (iError == NO_ERRORS)
    iError = read_rnrn(conn, buffer, sizeof(buffer), &pos, &bytes_read, 32000);

  if (iError == NO_ERRORS)
    process_get(conn, request);

  if (method)
    free(method);
  if (request)
    free(request);
  if (protocol)
    free(protocol);

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
      {
        send_ok(conn);
        int rc = showboard(conn, "Not Found");
      }
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
  char* pstr;

  if (service_detect("play=", &page)) // process_player
  {
    send_ok(conn);
    rc = player_show(conn, page);
  }
  else if ((pstr = strstr(page, "pump="))) // process_pump
  {
    send_ok(conn);
    rc = pump(conn, page);
  }
  else if (*page == '/' && *(page + 1) == '\0') // process_catalog
  {
    send_ok(conn);
    rc = showboard(conn, page);
  }
  else
  {
    send_ok(conn);
    rc = showboard(conn, page);
  }

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
      *pos = buf;
    }
    else
      *bytes_read -= cnt;
  }

  if (result)
    *(result + word_len) = '\0';
  return result;
}

int read_rnrn(const int conn, char* buf, const ssize_t buf_size, char** pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t total_read = *bytes_read;
  int i = 0, n = strlen("\r\n\r\n");
  char* pstr;

  while ((pstr = strstr(*pos, "\r\n\r\n")) == NULL)
  {
    if (total_read > read_limit)
    {
      fprintf(stderr, "Connection %d: limit exceeded while reading RNRN\n", conn);
      *bytes_read = 0;
      return BAD_REQUEST;
    }

    if (*bytes_read < n)
      n = *bytes_read;
    for (i = 0; i < n; i++)
      *(buf + i) = *(*pos + *bytes_read - n + i);
    *pos = buf + i;

    *bytes_read = read_block(conn, *pos, buf_size - i);
    total_read += *bytes_read;
    *pos = buf;
  }

  *bytes_read = *bytes_read + i - (pstr - *pos);
  *pos = pstr;
  return NO_ERRORS;
}

void send_ok(const int conn)
{
  char* ok_response =
    "HTTP/1.0 200 OK\n"
    "Content-type: text/html\n"
    "\n";

  write(conn, ok_response, strlen(ok_response));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", ok_response);
}

void send_bad_request(const int conn)
{
  char* bad_request_response =
    "HTTP/1.0 400 Bad Reguest\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Bad Request</h1>\n"
    "  <p>This server did not understand your request.</p>\n"
    " </body>\n"
    "</html>\n";

  write(conn, bad_request_response, strlen(bad_request_response));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", bad_request_response);
}

void send_bad_method(const int conn)
{
  char* bad_method_response =
    "HTTP/1.0 501 Method Not Implemented\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Method Not Implemented</h1>\n"
    "  <p>The requested method is not supported.</p>\n"
    " </body>\n"
    "</html>\n";

  write(conn, bad_method_response, strlen(bad_method_response));
  fprintf(stdout, "Sended to client:\n");
  fprintf(stdout, "%s", bad_method_response);
}

int service_detect(const char* service_token, const char** pos)
{
  const char* it = *pos;

  while (*service_token == *it && *service_token != '\0')
  {
    service_token++;
    it++;
  }

  if (*service_token == '\0')
  {
    *pos = it;
    return 1;
  }

  return 0;
}

char* get_self_executable_directory()
{
  int rc;
  char link_target[PATH_MAX];

  // Read symlink /proc/self/exe
  rc = readlink("/proc/self/exe", link_target, sizeof(link_target) - 1);
  if (rc == -1)
    return NULL;

  // Remove file name
  while (rc > 0 && link_target[rc] != '/')
    link_target[rc--] = '\0';

  return strndup(link_target, PATH_MAX);
}
