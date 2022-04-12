#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define   SERVER_PORT             5810
#define   SERVER_LISTEN_BACKLOG   32

#define   TINY_BUFFER_SIZE        8
#define   SMALL_BUFFER_SIZE       256
#define   STANDARD_BUFFER_SIZE    4096
#define   LARGE_BUFFER_SIZE       1048576

static char* ok_response =
  "HTTP/1.0 200 OK\n"
  "Content-type: text/html\n"
  "\n";

static char* bad_request_response =
  "HTTP/1.0 400 Bad Reguest\n"
  "Content-type: text/html\n"
  "\n"
  "<html>\n"
  " <body>\n"
  "  <h1>Bad Request</h1>\n"
  "  <p>This server did not understand your request.</p>\n"
  " </body>\n"
  "</html>\n";

static char* not_found_response =
  "HTTP/1.0 404 Not Found\n"
  "Content-type: text/html\n"
  "\n"
  "<html>\n"
  " <body>\n"
  "  <h1>Not Found</h1>\n"
  "  <p>The requested URL was not found.</p>\n"
  " </body>\n"
  "</html>\n";

static char* bad_method_response =
  "HTTP/1.0 501 Method Not Implemented\n"
  "Content-type: text/html\n"
  "\n"
  "<html>\n"
  " <body>\n"
  "  <h1>Method Not Implemented</h1>\n"
  "  <p>The requested method is not supported.</p>\n"
  " </body>\n"
  "</html>\n";

// catch SIGCHLD
void clean_up_child_process(int);
void process_connection(int conn);
void process_get(int conn, const char* page);
ssize_t read_block(const int conn, char* buf, const ssize_t buf_size);
char* skip_spaces(const int conn, char* buf, const ssize_t buf_size, char* pos, ssize_t* bytes_read, const ssize_t read_limit);
char* read_word(const int conn, char* buf, const ssize_t buf_size, char* pos, ssize_t* bytes_read, const ssize_t read_limit);
char* read_rnrn(const int conn, char* buf, const ssize_t buf_size, char* pos, ssize_t* bytes_read, const ssize_t read_limit);

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

      process_connection(connection);

      close(connection);
      return EXIT_SUCCESS;
    }
    else if (child_pid > 0) // Parent
    {
      close(connection);
    }
    else // fork() failed
    {
      fprintf(stderr, "Error in function \"fork\": %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

// catch SIGCHLD
void clean_up_child_process(int signal_number)
{
  int status;
  wait(&status);
}

void process_connection(int conn)
{
  char buffer[SMALL_BUFFER_SIZE];
  ssize_t bytes_read;
  char* pos;

  char* method = NULL;
  char* request = NULL;
  char* protocol = NULL;

  fprintf(stdout, "Connection %d: Rceived from client\n", conn);
  bytes_read = read_block(conn, buffer, sizeof(buffer) - 3);

  pos = buffer;

  pos = skip_spaces(conn, buffer, sizeof(buffer) - 3, pos, &bytes_read, 1000);
  method = read_word(conn, buffer, sizeof(buffer) - 3, pos, &bytes_read, 5);

  if (!method || strcmp(method, "GET") && strcmp(method, "POST"))
  {
    write(conn, bad_method_response, sizeof(bad_method_response));
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", bad_method_response);
  }

  pos = skip_spaces(conn, buffer, sizeof(buffer) - 3, pos, &bytes_read, 1000);
  request = read_word(conn, buffer, sizeof(buffer) - 3, pos, &bytes_read, 32000);
  pos = skip_spaces(conn, buffer, sizeof(buffer) - 3, pos, &bytes_read, 1000);
  protocol = read_word(conn, buffer, sizeof(buffer) - 3, pos, &bytes_read, 9);
  if (!protocol || strcmp(protocol, "HTTP/1.0") && strcmp(protocol, "HTTP/1.1"))
  {
    write(conn, bad_request_response, sizeof(bad_request_response));
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", bad_request_response);
  }

  pos = read_rnrn(conn, buffer, sizeof(buffer) - 3, pos, &bytes_read, 32000);

  process_get(conn, request);
}

void process_get(int conn, const char* page)
{
  int bFound = 0;
  // parse request here
  if (*page == '/' && strchr(page + 1, '/') == NULL)
  {
    bFound = 1;
  }

  // if requested service not found
  if (!bFound)
  {
    write(conn, not_found_response, sizeof(not_found_response));
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", not_found_response);
  }
  else // Correct request
  {
    write(conn, ok_response, sizeof(ok_response));
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", ok_response);

    // process_catalog
    // process_player
  }
}

ssize_t read_block(const int conn, char* buf, const ssize_t buf_size)
{
  ssize_t bytes_read;
  bytes_read = read(conn, buf, buf_size);
  if (bytes_read > 0)
  {
    *(buf + bytes_read) = '\0';
    fprintf(stdout, "%s", buf);
  }
  else if (bytes_read == 0)
  {
    close(conn);
    fprintf(stdout, "Connection %d terminated without sending any data\n", conn);
    exit(EXIT_SUCCESS);

  }
  else // (bytes_read == -1)
  {
    close(conn);
    fprintf(stderr, "Error in function \"read\": %s\n", strerror(errno));
    fprintf(stderr, "Connection %d unexpectedly terminated\n", conn);
    exit(EXIT_FAILURE);
  }
  return bytes_read;
}

char* skip_spaces(const int conn, char* buf, const ssize_t buf_size, char* pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t cnt, total_cnt = 0;

  while (*pos == ' ')
  {
    cnt = 1;

    while (*(++pos) == ' ' && ++cnt < *bytes_read);

    total_cnt += cnt;

    if (total_cnt > read_limit)
    {
      fprintf(stderr, "Connection %d: limit exceeded while skip spaces\n", conn);
      exit(EXIT_FAILURE);
    }

    if (cnt == *bytes_read)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      pos = buf;
    }
    else
      *bytes_read -= cnt;
  }
  return pos;
}

char* read_word(const int conn, char* buf, const ssize_t buf_size, char* pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  char* result = NULL;
  ssize_t word_len = 0, cnt;

  while (*pos != ' ' && *pos != '\r' && *pos != '\n')
  {
    cnt = 1;
    do pos++;
    while (*pos != ' ' && *pos != '\r' && *pos != '\n' && ++cnt < *bytes_read);

    word_len += cnt;

    if (word_len + 1 > read_limit)
    {
      if (result)
        free(result);
      fprintf(stderr, "Connection %d: limit exceeded while read_word\n", conn);
      exit(EXIT_FAILURE);
    }

    result = realloc(result, word_len + 1);
    memcpy(result + word_len - cnt, pos - cnt, cnt);

    if (cnt == *bytes_read)
    {
      *bytes_read = read_block(conn, buf, buf_size);
      pos = buf;
    }
    else
      *bytes_read -= cnt;
  }

  if (result)
    *(result + word_len) = '\0';
  return result;
}

char* read_rnrn(const int conn, char* buf, const ssize_t buf_size, char* pos, ssize_t* bytes_read, const ssize_t read_limit)
{
  ssize_t total_read = *bytes_read;

  while (strstr(pos, "\r\n\r\n") == NULL)
  {
    if (total_read > read_limit)
    {
      fprintf(stderr, "Connection %d: limit exceeded while reading RNRN\n", conn);
      exit(EXIT_FAILURE);
    }

    if (*bytes_read == 1)
    {
      *buf = *pos;
      pos = buf + 1;
    }
    else if (*bytes_read > 1)
    {
      *buf = *(pos + *bytes_read - 2);
      *(buf + 1) = *(pos + *bytes_read - 1);
      pos = buf + 2;
    }

    *bytes_read = read_block(conn, pos, buf_size);
    total_read += *bytes_read;
    pos = buf;
  }

  return pos;
}
