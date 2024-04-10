#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 32
#define BUFFER_SIZE 2048
#define UINT16_MAX 65535
#define TEN 10
#define MAX_USERNAME_LENGTH 256
#define MAX_USER_INFO_LENGTH (MAX_USERNAME_LENGTH + 2)
#define FIVE 5

struct ClientInfo {
  int client_socket;
  int client_index;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static char usernames[MAX_CLIENTS][MAX_USERNAME_LENGTH] = {0};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static int clients[MAX_CLIENTS] = {0};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool connection_message_sent[MAX_CLIENTS] = {false};

static void *handle_client(void *arg);
noreturn void start_server(const char *address, uint16_t port);
void send_user_list(int client_socket);
void send_message_to_client(int client_socket, uint8_t protocol_version,
                            const char *message);

// void          send_uint8_to_client(int client_socket, uint8_t value);
// void          send_uint16_to_client(int client_socket, uint16_t value);
// ssize_t       send_string_to_client(int client_socket, const char *str);

// void send_uint8_to_client(int client_socket, uint8_t value)
//{
//     uint8_t network_order = value;
//     send(client_socket, &network_order, sizeof(uint8_t), 0);
// }
//
// void send_uint16_to_client(int client_socket, uint16_t value)
//{
//     uint16_t network_order = htons(value);
//     send(client_socket, &network_order, sizeof(uint16_t), 0);
// }
//
// ssize_t send_string_to_client(int client_socket, const char *str)
//{
//     size_t  length     = strlen(str);
//     ssize_t total_sent = 0;
//
//     while((size_t)total_sent < length)
//     {
//         ssize_t sent = send(client_socket, str + total_sent, length -
//         (size_t)total_sent, 0); if(sent == -1)
//         {
//             perror("send_string_to_client");
//             return -1;    // Error occurred
//         }
//         if(sent == 0)
//         {
//             // Connection closed by client
//             return total_sent;
//         }
//         total_sent += sent;
//     }
//     return total_sent;
// }

void send_message_to_client(int client_socket, uint8_t protocol_version,
                            const char *message) {
  uint16_t message_size = htons(strlen(message));
  send(client_socket, &protocol_version, sizeof(uint8_t), 0);
  send(client_socket, &message_size, sizeof(uint16_t), 0);
  send(client_socket, message, strlen(message), 0);
}

void *handle_client(void *arg) {
  struct ClientInfo *client_info = (struct ClientInfo *)arg;
  int client_socket = client_info->client_socket;
  int client_index = client_info->client_index;
  char username_change_message[BUFFER_SIZE];
  char *message_start;
  char *space_ptr;
  const char *recipient_username;
  const char *message_content;
  char username_assigned_message[BUFFER_SIZE];

  if (usernames[client_index][0] == '\0') {
    char index_str[FIVE];

    strcpy(usernames[client_index], "Client ");
    snprintf(index_str, FIVE, "%d", client_index);
    strcat(usernames[client_index], index_str);
  }

  memset(username_assigned_message, 0, BUFFER_SIZE);
  strcpy(username_assigned_message, "Your username has been set to: ");
  strcat(username_assigned_message, usernames[client_index]);
  strcat(username_assigned_message, "\n");

  if (!connection_message_sent[client_index]) {
    char new_connection_message[BUFFER_SIZE];
    strcpy(new_connection_message, usernames[client_index]);
    strcat(new_connection_message, " has joined the chat.\n");

    // Notify the new client about its username
    send_message_to_client(clients[client_index], 1, username_assigned_message);

    // Notify all existing clients about the new client's connection
    for (int i = 0; i < MAX_CLIENTS; ++i) {
      if (clients[i] != 0 && i != client_index) {
        send_message_to_client(clients[i], 1, new_connection_message);
      }
    }

    connection_message_sent[client_index] = true;
  }

  while (1) {
    uint8_t buffer_uint8;
    uint16_t buffer_uint16;
    char buffer[BUFFER_SIZE];

    ssize_t bytes_received =
        recv(client_socket, &buffer_uint8, sizeof(uint8_t), 0);
    if (bytes_received <= 0) {
      if (bytes_received < 0) {
        perror("Error receiving uint8 from client");
      }
      printf("%s disconnected.\n", usernames[client_index]);
      close(client_socket);
      clients[client_index] = 0;
      free(client_info);
      pthread_exit(NULL);
    }

    printf("Received uint8 from %s: %u\n", usernames[client_index],
           buffer_uint8);

    bytes_received = recv(client_socket, &buffer_uint16, sizeof(uint16_t), 0);
    if (bytes_received <= 0) {
      if (bytes_received < 0) {
        perror("Error receiving uint16 from client");
      }
      printf("Client %s disconnected.\n", usernames[client_index]);
      close(client_socket);
      clients[client_index] = 0;
      free(client_info);
      pthread_exit(NULL);
    }
    buffer_uint16 = ntohs(buffer_uint16);
    printf("Received uint16 from %s: %u\n", usernames[client_index],
           buffer_uint16);

    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
      if (bytes_received < 0) {
        perror("Error receiving string from client");
      }
      printf("Client %s disconnected.\n", usernames[client_index]);
      close(client_socket);
      clients[client_index] = 0;
      free(client_info);
      pthread_exit(NULL);
    } else {
      buffer[bytes_received] = '\0';
      if (buffer[bytes_received - 1] == '\n') {
        buffer[bytes_received - 1] = '\0';
      }
    }

    printf("Received string from %s: %s\n", usernames[client_index], buffer);

    if (buffer[0] == '/') {

      if (strncmp(buffer, "/u ", 3) == 0) {
        char confirmation_message[BUFFER_SIZE];
        char *new_username;
        if (strchr(buffer + 3, ' ') != NULL) {
          send_message_to_client(
              client_socket, 1,
              "Invalid username. Please enter a username without spaces.\n");
          continue;
        }
        new_username = buffer + 3;
        new_username[strcspn(new_username, "\n")] = '\0';

        strcpy(username_change_message, usernames[client_index]);
        strcat(username_change_message, " has changed their username to: ");
        strcat(username_change_message, new_username);
        strcat(username_change_message, "\n");

        strncpy(usernames[client_index], new_username, MAX_USERNAME_LENGTH);
        usernames[client_index][MAX_USERNAME_LENGTH - 1] = '\0';

        strcpy(confirmation_message, "Your username has been set to: ");
        strcat(confirmation_message, new_username);
        strcat(confirmation_message, "\n");
        send_message_to_client(client_socket, 1, confirmation_message);

        for (int i = 0; i < MAX_CLIENTS; ++i) {
          if (clients[i] != 0 && i != client_index) {
            send_message_to_client(client_socket, 1, username_change_message);
          }
        }
      } else if (strncmp(buffer, "/ul", 3) == 0) {
        send_user_list(client_socket);
      } else if (strncmp(buffer, "/w ", 3) == 0) {
        int recipient_index;

        // Parsing recipient username and message content
        message_start = buffer + 3;
        space_ptr = strchr(message_start, ' ');
        if (space_ptr == NULL) {
          send_message_to_client(
              client_socket, 1,
              "Invalid whisper command. Usage: /w <username> <message>\n");
          continue;
        }
        // Null-terminate recipient username
        *space_ptr = '\0';
        recipient_username = message_start;
        message_content = space_ptr + 1;

        // Searching for recipient's index in the clients array
        recipient_index = -1;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
          if (clients[i] != 0 &&
              strcmp(usernames[i], recipient_username) == 0) {
            recipient_index = i;
            break;
          }
        }

        // If recipient not found or offline, notify the sender
        if (recipient_index != -1) {
          // Construct whisper message
          char whisper_message[BUFFER_SIZE];
          snprintf(whisper_message, BUFFER_SIZE, "Whisper from %s: %s\n",
                   usernames[client_index], message_content);

          // Send whisper message to recipient
          send_message_to_client(clients[recipient_index], 1, whisper_message);
        } else {
          // Send error message to sender
          send_message_to_client(client_index, 1,
                                 "Recipient not found or offline.\n");
          printf("Recipient '%s' not found or offline. Whisper not sent.\n",
                 recipient_username);
        }
      }

      else if (strncmp(buffer, "/h", 2) == 0) {
        const char helpMessage[BUFFER_SIZE] =
            ">>>> Available commands:\n/u <Preferred Username> - Change "
            "usernames\n/ul - Shows the list of users\n/w <username> "
            "<message>- Whisper to a User\n/h - This help message\n\nCtrl + C "
            "- Leave the Server\n";
        send_message_to_client(client_socket, 1, helpMessage);
      } else {
        send_message_to_client(client_index, 1, "Invalid command.\n");
      }
    } else {
      for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != 0 && i != client_index) {
          char formatted_message[BUFFER_SIZE];
          strcpy(formatted_message, usernames[client_index]);
          strcat(formatted_message, ": ");
          strcat(formatted_message, buffer);
          strcat(formatted_message, "\n");
          printf("Formatted message to client %s: %s\n", usernames[i],
                 formatted_message);

          send_message_to_client(clients[i], 1, formatted_message);
        }
      }
    }
  }
}

void start_server(const char *address, uint16_t port) {
  int server_socket;
  struct sockaddr_in server_addr;
  int optval = 1;

#ifdef SOCK_CLOEXEC
  server_socket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
#else
  server_socket =
      socket(AF_INET, SOCK_STREAM, 0); // NOLINT(android-cloexec-socket)
#endif

  if (server_socket == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(address);
  server_addr.sin_port = htons(port);

  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval,
                 sizeof(optval)) == -1) {
    perror("Setsockopt failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  if (bind(server_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) == -1) {
    perror("Bind failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  if (listen(server_socket, MAX_CLIENTS) == -1) {
    perror("Listen failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on %s:%d\n", address, port);

  while (1) {
    int client_socket;
    int client_index;
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    pthread_t tid;
    struct ClientInfo *client_info =
        (struct ClientInfo *)malloc(sizeof(struct ClientInfo));

    client_socket = accept(server_socket, (struct sockaddr *)&client_addr,
                           (socklen_t *)&client_len);
    if (client_socket == -1) {
      perror("Accept failed");
      continue;
    }

    printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    for (client_index = 0; client_index < MAX_CLIENTS; ++client_index) {
      if (clients[client_index] == 0) {
        clients[client_index] = client_socket;
        break;
      }
    }

    if (client_index == MAX_CLIENTS) {
      fprintf(stderr, "Too many clients. Connection rejected.\n");
      close(client_socket);
      continue;
    }

    if (client_info == NULL) {
      perror("Memory allocation failed");
      close(client_socket);
      exit(EXIT_FAILURE);
    }

    client_info->client_socket = client_socket;
    client_info->client_index = client_index;

    if (pthread_create(&tid, NULL, handle_client, (void *)client_info) != 0) {
      perror("Thread creation failed");
      close(client_socket);
      free(client_info);
      exit(EXIT_FAILURE);
    }

    pthread_detach(tid);
  }
}

void send_user_list(int client_socket) {
  char user_list[BUFFER_SIZE];
  memset(user_list, 0, BUFFER_SIZE);
  strcpy(user_list, ">>>> Connected users:\n");
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i] != 0 && usernames[i][0] != '\0') {
      char user_info[MAX_USER_INFO_LENGTH];
      strcpy(user_info, usernames[i]);
      strcat(user_info, "\n");
      strncat(user_list, user_info, sizeof(user_list) - strlen(user_list) - 1);
    }
  }
  send_message_to_client(client_socket, 1, user_list);
}

int main(int argc, char *argv[]) {
  if (argc == 3 || (argc == 4 && strtol(argv[3], NULL, TEN) > 0)) {
    char *endptr;
    long int port_long = strtol(argv[argc - 1], &endptr, TEN);
    if (*endptr != '\0' || port_long < 0 || port_long > UINT16_MAX) {
      fprintf(stderr, "Invalid port number: %s\n", argv[argc - 1]);
      exit(EXIT_FAILURE);
    }
    start_server(argv[1], (uint16_t)port_long);
  } else {
    fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
}
