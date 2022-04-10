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

// catch SIGCHLD
void clean_up_child_process(int);

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

      //handle_connection(connection);

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
