#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PASSWORD_LENGTH 256
#define BUFFER_SIZE 100
#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6
#define BASE 10
#define MAX_PORT 65535

void write_ack_to_socket(int sockfd, const char *ack_message);

void write_ack_to_socket(int sockfd, const char *ack_message) {
  size_t message_len = strlen(ack_message);
  uint16_t size = htons((uint16_t)message_len);
  uint8_t version = 1;

  // Write protocol version
  if (write(sockfd, &version, sizeof(uint8_t)) == -1) {
    perror("Error writing protocol version to socket");
    exit(EXIT_FAILURE);
  }

  // Write the size of the acknowledgment message
  if (write(sockfd, &size, sizeof(uint16_t)) == -1) {
    perror("Error writing acknowledgment message size to socket");
    exit(EXIT_FAILURE);
  }

  // Write the acknowledgment message
  if (write(sockfd, ack_message, message_len) == -1) {
    perror("Error writing acknowledgment message to socket");
    exit(EXIT_FAILURE);
  }
}

int main(void) {
  pid_t pid;
  int pipefd[2];
  int server_started = 0;
  char command[3];

  // Create a pipe for IPC
  // NOLINTNEXTLINE(android-cloexec-pipe)
  if (pipe(pipefd) == -1) {
    perror("Pipe creation failed");
    exit(EXIT_FAILURE);
  }

  printf("Enter 1 to wait for the server manager or 2 to start the process "
         "without the server manager: ");

  while (1) {
    fgets(command, sizeof(command), stdin);

    if (command[0] == '1') {
      char ip[MAX_IP_LENGTH];
      char port[MAX_PORT_LENGTH];
      struct sockaddr_in servaddr;
      long int port_num;
      char *endptr;
      int sockfd;
      struct sockaddr_in cliaddr;
      socklen_t len;
      int connfd;
      uint8_t password_length_uint8;
      uint16_t password_length_uint16;
      char password[MAX_PASSWORD_LENGTH + 1];
      ssize_t n;
      const char *ack_messageACCEPT = "ACCEPTED";
      const char *ack_messageSTARTED = "STARTED";
      const char *ack_messageSTOPPED = "STOPPED";
      pid_t server_pid = -1;
      int opt = 1;

      // Read the uint8_t, uint16_t, and string for the command
      uint8_t command_length_uint8;
      uint16_t command_length_uint16;
      char buffer[BUFFER_SIZE + 1]; // Add 1 for the null terminator

      printf("Enter IP address: ");
      fgets(ip, sizeof(ip), stdin);
      ip[strcspn(ip, "\n")] = 0; // Remove newline character

      printf("Enter port number: ");
      fgets(port, sizeof(port), stdin);
      port[strcspn(port, "\n")] = 0; // Remove newline character

#ifdef SOCK_CLOEXEC
      sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0); // NOLINT
      if (sockfd == -1) {
        fprintf(stderr, "Invalid sockfd number: %d\n", sockfd);
        exit(EXIT_FAILURE);
      }
#else
      sockfd = socket(AF_INET, SOCK_STREAM, 0); // NOLINT
#endif

      // Add this line to set the SO_REUSEADDR option
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
          -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
      }

      // Bind the socket to the IP and port

      servaddr.sin_family = AF_INET;
      servaddr.sin_addr.s_addr = inet_addr(ip);
      port_num = strtol(port, &endptr, BASE);

      // Check for conversion errors
      if (*endptr != '\0') {
        fprintf(stderr, "Invalid port number: %s\n", port);
        exit(EXIT_FAILURE);
      }

      // Check for out-of-range values
      if (port_num < 0 || port_num > MAX_PORT) {
        fprintf(stderr, "Port number out of range: %ld\n", port_num);
        exit(EXIT_FAILURE);
      }

      servaddr.sin_port = htons((uint16_t)port_num);

      if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
      }

      // Listen for a client
      if (listen(sockfd, 1) ==
          -1) // NOLINT (-Werror=analyzer-fd-use-without-check)
      {
        perror("Listen failed");
        exit(EXIT_FAILURE);
      }

      // Accept a client
      len = sizeof(cliaddr);
      connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
      if (connfd == -1) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
      }

      // Read the uint8_t, uint16_t, and string for the PASSWORD

      n = recv(connfd, &password_length_uint8, sizeof(uint8_t), 0);
      if (n <= 0) {
        perror("Error receiving uint8 for password length from client");
        exit(EXIT_FAILURE);
      }

      n = recv(connfd, &password_length_uint16, sizeof(uint16_t), 0);
      if (n <= 0) {
        perror("Error receiving uint16 for password length from client");
        exit(EXIT_FAILURE);
      }
      password_length_uint16 =
          ntohs(password_length_uint16); // Convert from network byte order to
                                         // host byte order

      n = recv(connfd, password, password_length_uint16, 0);
      if (n <= 0) {
        perror("Error receiving password from client");
        exit(EXIT_FAILURE);
      }
      password[n] = '\0'; // Null-terminate the string

      // Got rid of $s\n
      printf("Received password from Server Manager: %s", password);

      // Compare the received password with the expected password
      // GET RID OF \N
      if (strcmp(password, "hellyabrother") != 0) {
        printf("Incorrect password received.\n");
        // Handle incorrect password scenario here
        exit(EXIT_FAILURE);
      }

      printf("\n");
      printf("Correct password received.\n");
      printf("Server manager successfully joined. \n");

      // Send acceptance ack
      write_ack_to_socket(connfd, ack_messageACCEPT);

      while (1) {
        // COMMAND RECIEVING
        n = recv(connfd, &command_length_uint8, sizeof(uint8_t), 0);
        if (n <= 0) {
          perror("Error receiving uint8 for command length from client");
          exit(EXIT_FAILURE);
        }

        n = recv(connfd, &command_length_uint16, sizeof(uint16_t), 0);
        if (n <= 0) {
          perror("Error receiving uint16 for command length from client");
          exit(EXIT_FAILURE);
        }
        command_length_uint16 = ntohs(command_length_uint16);

        n = recv(connfd, buffer, command_length_uint16, 0);
        if (n <= 0) {
          perror("Error receiving command from client");
          exit(EXIT_FAILURE);
        }
        buffer[n] = '\0'; // Null-terminate the string
        printf("Received command from client: %s\n", buffer);

        // GET RID OF \N
        if (strcmp(buffer, "/q") == 0) {
          // Send acknowledgment to the client
          write_ack_to_socket(connfd, ack_messageSTOPPED);

          if (server_started) {
            printf("Stopping the server\n");
            if (server_pid != -1) {
              kill(server_pid,
                   SIGTERM); // Send a SIGTERM signal to the server process
              waitpid(server_pid, NULL, 0);
              server_pid = -1;
            }
            server_started = 0; // Update the flag
          } else {
            write_ack_to_socket(connfd, "Server not started");
            printf("Server is not on!! Turn it on with /s");
            continue;
          }
        }
        // Check if the received command is "/s"
        else if (strcmp(buffer, "/s") == 0) {
          // Send acknowledgment to the client
          write_ack_to_socket(connfd, ack_messageSTARTED);

          // Fork a process to run server.c
          pid = fork();
          if (pid == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
          } else if (pid == 0) {
            // Child process
            // Increment the port number for the forked server process
            int new_port = (int)(port_num) + 1;
            char new_port_str[MAX_PORT_LENGTH];
            sprintf(new_port_str, "%d", new_port);

            printf("Starting the server\n");

            execl("./server", "server", ip, new_port_str, (char *)NULL);
            perror("Exec failed");
            exit(EXIT_FAILURE);
          } else {
            server_pid = pid;
            server_started =
                1; // Set the flag to indicate that the server has started
          }
          // If the server has started, stop accepting new connections
          if (server_started) {
            // Close the server socket
            close(sockfd);
          }
        } else {
          printf("Invalid command received from client.\n");
        }
      }
    } else if (command[0] == '2') {
      char ip[MAX_IP_LENGTH];
      char port[MAX_PORT_LENGTH];
      printf("Starting the process without the server manager...\n");

      printf("Enter IP address: ");
      fgets(ip, sizeof(ip), stdin);
      ip[strcspn(ip, "\n")] = 0; // Remove newline character

      printf("Enter port number: ");
      fgets(port, sizeof(port), stdin);
      port[strcspn(port, "\n")] = 0; // Remove newline character

      // Fork a process to run server.c
      pid = fork();
      if (pid == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
      } else if (pid == 0) {
        // Child process
        execl("./server", "server", ip, port, (char *)NULL);
        perror(
            "Exec failed"); // This line should not be reached if exec succeeds
        exit(EXIT_FAILURE);
      } else {
        // Parent process
        wait(NULL); // parent waits for the child to finish
      }
    } else {
      printf("Invalid command.\n");
    }
  }
}
