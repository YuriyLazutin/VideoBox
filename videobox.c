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
void process_connection(int connection_fd);
void process_get(int connection_fd, const char* page);

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

void process_connection(int connection_fd)
{
  fprintf(stdout, "Connection %d: Waiting data from client\n", connection_fd);

  char buffer[256];
  ssize_t bytes_read;

  bytes_read = read(connection_fd, buffer, sizeof(buffer) - 3);
  fprintf(stdout, "Rceived from client:\n");
  fprintf(stdout, "%s", buffer);
  if (bytes_read > 0)
  {
    char method[sizeof(buffer)];
    char url[sizeof(buffer)];
    char protocol[sizeof(buffer)];

    buffer[bytes_read] = '\0';
    sscanf(buffer, "%s %s %s", method, url, protocol); // Secure alert! Provided strings can be more than buffer size

    while (strstr(buffer, "\r\n\r\n") == NULL)
    {
      char* pbuf = buffer;
      if (bytes_read == 1)
        pbuf++;
      else if (bytes_read > 1)
      {
        *pbuf++ = buffer[bytes_read - 2];
        *pbuf++ = buffer[bytes_read - 1];
      }
      bytes_read = read(connection_fd, pbuf, sizeof(buffer) - 3);
      *(pbuf + bytes_read) = '\0';
      fprintf(stdout, "%s", buffer);
    }

    if (bytes_read == -1) // Connection unexpectedly terminated
    {
      close(connection_fd);
      return;
    }

    if (strcmp(protocol, "HTTP/1.0") && strcmp(protocol, "HTTP/1.1"))
    {
      write(connection_fd, bad_request_response, sizeof(bad_request_response));
      fprintf(stdout, "Sended to client:\n");
      fprintf(stdout, "%s", bad_request_response);
    }
    else if (strcmp(method, "GET"))
    {
      write(connection_fd, bad_method_response, sizeof(bad_method_response));
      fprintf(stdout, "Sended to client:\n");
      fprintf(stdout, "%s", bad_method_response);
    }
    else
      process_get(connection_fd, url);
  }
  else if (bytes_read == 0) // Connection terminated without sending any data
    // Nothing to do
  ;

  else // read() failed
  {
    fprintf(stderr, "Error in function \"read\": %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
}

void process_get(int connection_fd, const char* page)
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
    write(connection_fd, not_found_response, sizeof(not_found_response));
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", not_found_response);
  }
  else // Correct request
  {
    write(connection_fd, ok_response, sizeof(ok_response));
    fprintf(stdout, "Sended to client:\n");
    fprintf(stdout, "%s", ok_response);

    // process_catalog
    // process_player
  }
}
