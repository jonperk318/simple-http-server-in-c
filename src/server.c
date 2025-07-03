#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


// Constants
const int PORT = 3001;
const int CONNECTIONS = 10;
const int BSTRING_INIT_CAPACITY = 16;
const char HTTP_VERSION[] = "HTTP/1.1";
const int BUFFER_SIZE = 1024;
const int MAX_HEADERS = 128;


// Better string struct
struct BString {
    char *data;
    size_t length;
    size_t capacity;
};

struct Bstring *bstring_init(size_t capacity, const char *const s);
bool bstring_append(struct Bstring *self, const char *const s);
void bstring_free(struct Bstring *self);

struct Bstring *file = NULL;


// HTTP methods
enum HTTPMethodType {
    HTTP_UNKNOWN,
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_PATCH,
    HTTP_DELETE,
};

struct HTTPMethod {
    const char *const string;
    enum HTTPMethodType type;
};

struct HTTPMethod HTTP_METHODS[] = {
    {.string = "GET", .type = HTTP_GET},
    {.string = "POST", .type = HTTP_POST},
    {.string = "PUT", .type = HTTP_PUT},
    {.string = "PATCH", .type = HTTP_PATCH},
    {.string = "DELETE", .type = HTTP_DELETE},
};

size_t HTTP_METHODS_LEN =
    sizeof(HTTP_METHODS) / sizeof(HTTP_METHODS[0]);

struct HTTPHeader {
    char *key;
    char *value;
};

struct HTTPRequest {
    enum HTTPMethodType method;
    char *path;
    struct HTTPHeader *headers;
    size_t headers_len;
    char *_buffer;
    size_t _buffer_len;
    struct BString *body;
};

// Connection
void handle_connection(int conn_fd);
int server_fd = -1;
struct sockaddr_in client_addr;
socklen_t client_addr_len = sizeof(client_addr);


// MAIN
int main(int argc, char **argv) {

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    int connection_backlog = 5;

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(4221),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    // Set directory for public files
    if (argc > 2) {
        file = bstring_init(0, argv[2]);
        if (argv[2][strlen(argv[2]) - 1] != '/') {
            bstring_append(file, "/");
        }
    } else {
        file = bstring_init(0, "./public/");
    }

    // Handle error with server file descriptor
    if (server_fd == -1) {
        printf("Error creating socket: %s\n", strerror(errno));
        goto cleanup;
    }

    // Allow reuse of address upon server restart by forcing socket to connect to port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
          0) {
        printf("Error forcing reuse of address: %s\n", strerror(errno));
        goto cleanup;
    }

    // Check if bind failed
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        printf("Error binding socket to address: %s\n", strerror(errno));
        goto cleanup;
    }

    // Check if listen failed
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Error listening for connection: %s\n", strerror(errno));
        goto cleanup;
    }

    printf("Connection ready\n");

    // Cleanup
    cleanup:
      if (server_fd != -1) {
          close(server_fd);
      }

      if (file != NULL) {
          bstring_free(file);
      }

    return 0;
}
