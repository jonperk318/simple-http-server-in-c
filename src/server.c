#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>


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

struct Bstring *filename = NULL;


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

    // Set directory for public files
    if (argc > 2) {
        filename = bstring_init(0, argv[2]);
        if (argv[2][strlen(argv[2]) - 1] != '/') {
            bstring_append(filename, "/");
        }
    } else {
        filename = bstring_init(0, "./public/");
    }

    //printf("Filename: %s\n", filename->data);
    printf("Hello, World!\n");
    return 0;
}
