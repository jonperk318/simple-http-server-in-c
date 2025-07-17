#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>


// ********** CONSTANTS AND VARIABLES ********** //
const int PORT = 3000;
const int REUSE = 1;
const int CONNECTION_BACKLOG = 5;
const int STRING_INIT_CAPACITY = 16;
const char HTTP_VERSION[] = "HTTP/1.1";
const int BUFFER_SIZE = 1024;
const int MAX_HEADERS = 128;
int server_fd = -1;


// ********** DYNAMICALLY ALLOCATED STRING ********** //
struct String {
    char *data;
    size_t length;
    size_t capacity;
};

struct String *string_init(size_t capacity, const char *const s);
bool string_append(struct String *self, const char *const s);
void string_free(struct String *self);

struct String *file = NULL;


// ********** HTTP METHODS ********** //
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
    struct String *body;
};

char *http_get_header(struct HTTPRequest *self, char *header) {
    assert(self != NULL);
    assert(header != NULL);
    for (size_t i = 0; i < self->headers_len; ++i) {
        if (strcasecmp(header, self->headers[i].key) == 0) {
            return self->headers[i].value;
        }
    }
    return NULL;
}


// ********** READ HTML FILE ********** //
char *read_html_file(const char* filename) {

    FILE *fp = fopen (filename, "rb");
    if (fp == NULL) perror("Error opening HTML file");

    fseek(fp , 0L , SEEK_END);
    long lSize = ftell(fp);
    rewind(fp);

    char *content = calloc(1, lSize + 1);
    if (content == NULL) {
        fclose(fp);
        fputs("Memory allocation failed", stderr);
    }
    if (fread(content, lSize, 1, fp) != 1) {
        fclose(fp);
        free(content);
        fputs("Failed to read HTML file", stderr);
    }

    fclose(fp);
    return content;
}


// ********** HANDLING THE CONNECTION ********** //
void handle_connection(int conn_fd);

struct sockaddr_in client_addr;
socklen_t client_addr_len = sizeof(client_addr);

void *_handle_connection(void *conn_fd_ptr) {

    assert(conn_fd_ptr != NULL);
    int conn_fd = *(int *)conn_fd_ptr;
    printf("Processing connection with file descriptor: %d\n", conn_fd);

    // Reading data from the client
    struct HTTPRequest req = {0};
    req._buffer = malloc(BUFFER_SIZE);
    FILE *fp = NULL;
    size_t orig_file_len = file->length;

    if (req._buffer == NULL) {
        printf("Error allocating memory.\n");
        goto cleanup;
    }

    ssize_t bytes_read = recv(conn_fd, req._buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read == -1) {
        printf("Error with recv: %s \n", strerror(errno));
        goto cleanup;
    } else if (bytes_read == 0) {
        printf("Error with recv: connection was closed by the client.\n");
        goto cleanup;
    }

    // Make _buffer a null terminated string
    req._buffer[bytes_read] = '\0';
    req._buffer_len = (size_t)bytes_read;
    char *parse_buffer = req._buffer;

    // Parse and store HTTP method
    char *method = strsep(&parse_buffer, " ");
    for (size_t i = 0; i < HTTP_METHODS_LEN; ++i) {
        if (strcmp(method, HTTP_METHODS[i].string) == 0) {
            req.method = HTTP_METHODS[i].type;
            break;
        }
    }

    if (req.method == HTTP_UNKNOWN) {
        printf("Error, unknown HTTP method: %s\n", method);
        goto cleanup;
    }

    // Parse path
    req.path = strsep(&parse_buffer, " ");

    // Parse HTTP version
    strsep(&parse_buffer, "\r");
    parse_buffer++; // consume empty lines

    // Parse and store headers
    req.headers = malloc(sizeof(struct HTTPHeader) * MAX_HEADERS);
    req.headers_len = 0;

    for (size_t i = 0; i < MAX_HEADERS; ++i) {

        char *header_line = strsep(&parse_buffer, "\r");
        parse_buffer++;
        if (header_line[0] == '\0') { break; }

        char *key = strsep(&header_line, ":");
        header_line++;
        char *value = strsep(&header_line, "\0");
        req.headers[i].key = key;
        req.headers[i].value = value;
        ++req.headers_len;
    }

    // Parse request body
    size_t content_len = 0;
    char *content_len_header = http_get_header(&req, "content-length");
    if (content_len_header != NULL) {
        errno = 0;
        size_t result = strtoul(content_len_header, NULL, 10);
        if (errno == 0) {
            content_len = result;
        }
    }

    if (content_len > 0) {
        req.body = string_init(content_len + 1, NULL);
        string_append(req.body, parse_buffer);
        if (req.body->length != content_len) {
            printf("Error: content length does not match request");
            goto cleanup;
        }
    }

    int bytes_sent = -1;

    // Send response
    if (strcmp(req.path, "/") == 0) {
        struct String *res = string_init(0, "HTTP/1.1 200 OK\r\n\r\n");
        string_append(res, read_html_file("./public/index.html"));
        bytes_sent = send(conn_fd, res->data, res->length, 0);

    } else if (strcmp(req.path, "/user-agent") == 0) {

        char *s = http_get_header(&req, "user-agent");
        if (s == NULL) {
            s = "NULL";
        }

        size_t s_len = strlen(s);

        char *res = malloc(BUFFER_SIZE);
        sprintf(res,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length:%zu\r\n\r\n"
            "%s",
            s_len, s);

        bytes_sent = send(conn_fd, res, strlen(res), 0);
        free(res);

    } else if (strncmp(req.path, "/echo/", 6) == 0) {

        char *s = req.path + 6;
        size_t s_len = strlen(s);

        char *res = malloc(BUFFER_SIZE);
        sprintf(res,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length:%zu\r\n\r\n"
            "%s",
            s_len, s);

        bytes_sent = send(conn_fd, res, strlen(res), 0);
        free(res);

    } else if (req.method == HTTP_GET && strncmp(req.path, "/files/", 7) == 0) {

        string_append(file, req.path + 7);
        errno = 0;

        // Open the file
        fp = fopen(file->data, "r");
        if (fp == NULL && errno != ENOENT) {
            struct String *res = string_init(0, "HTTP/1.1 400 Bad Request\r\n\r\n");
            string_append(res, read_html_file("./public/400.html"));
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            perror("Error with fopen()");
            goto cleanup;
        }
        struct String *res = string_init(0, HTTP_VERSION);

        // File does not exist
        if (errno == ENOENT) {
            struct String *res = string_init(0, "HTTP/1.1 404 Not Found\r\n\r\n");
            string_append(res, read_html_file("./public/404.html"));
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            goto cleanup;
        }

        // Error with going to end of file
        if (fseek(fp, 0, SEEK_END) != 0) {
            struct String *res = string_init(0, "HTTP/1.1 400 Bad Request\r\n\r\n");
            string_append(res, read_html_file("./public/400.html"));
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            perror("Error with fseek()");
            goto cleanup;
        }

        // Error with getting file size
        long file_size = ftell(fp);
        if (file_size < 0) {
            struct String *res = string_init(0, "HTTP/1.1 400 Bad Request\r\n\r\n");
            string_append(res, read_html_file("./public/400.html"));
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            perror("Error with fseek()");
            goto cleanup;
        }

        // Error with going to beginning of file
        errno = 0;
        rewind(fp);
        if (errno != 0) {
            struct String *res = string_init(0, "HTTP/1.1 400 Bad Request\r\n\r\n");
            string_append(res, read_html_file("./public/400.html"));
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            perror("Error with rewind()");
            goto cleanup;
        }

        char file_size_str[sizeof(file_size) + 1];
        sprintf(file_size_str, "%ld", file_size);

        string_append(res, " 200 OK\r\n");
        string_append(res, "Content-Type: application/octet-stream\r\n");
        string_append(res, "Content-Length: ");
        string_append(res, file_size_str);
        string_append(res, "\r\n\r\n");

        bytes_sent = send(conn_fd, res->data, res->length, 0);
        string_free(res);

        char buffer[BUFFER_SIZE];

        while (file_size > 0) {

            long bytes_to_read = file_size > BUFFER_SIZE ? BUFFER_SIZE : file_size;
            size_t bytes_read = fread(buffer, bytes_to_read, 1, fp);

            // Check if bytes_read is less than expected due to error
            if (bytes_read < bytes_to_read && ferror(fp)) {
                struct String *res = string_init(0, "HTTP/1.1 400 Bad Request\r\n\r\n");
                string_append(res, read_html_file("./public/400.html"));
                bytes_sent = send(conn_fd, res->data, res->length, 0);
                puts("Error with fread()");
                goto cleanup;
            }

            bytes_sent = send(conn_fd, buffer, bytes_to_read, 0);
            file_size -= bytes_to_read;
        }

    } else if (req.method == HTTP_POST && strncmp(req.path, "/files/", 7) == 0) {

        string_append(file, req.path + 7);

        // Check if file already exists
        errno = 0;
        fp = fopen(file->data, "r");
        if (fp != NULL) {
            struct String *res = string_init(0, "HTTP/1.1 409 Conflict\r\n\r\n");
            string_append(res, read_html_file("./public/400.html"));
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            goto cleanup;
        }

        // Open file to write
        errno = 0;
        fp = fopen(file->data, "w");
        if (fp == NULL) {
            struct String *res = string_init(0, "HTTP/1.1 400 Bad Request\r\n\r\n");
            string_append(res, read_html_file("./public/400.html"));
            bytes_sent = send(conn_fd, res->data, res->length, 0);
            perror("Error with fopen()");
            goto cleanup;
        }

        size_t num_chunks = req.body->length / BUFFER_SIZE;
        if (num_chunks > 0) {
            fwrite(req.body->data, BUFFER_SIZE, num_chunks, fp);
        }

        size_t remaining_bytes = req.body->length % BUFFER_SIZE;
        if (remaining_bytes > 0) {
            fwrite(&req.body->data[req.body->length - remaining_bytes],
                remaining_bytes, 1, fp);
        }

        const char res[] = "HTTP/1.1 201 Created\r\n\r\n";
        bytes_sent = send(conn_fd, res, sizeof(res) - 1, 0);

    } else {

        struct String *res = string_init(0, "HTTP/1.1 404 Not Found\r\n\r\n");
        string_append(res, read_html_file("./public/404.html"));
        bytes_sent = send(conn_fd, res->data, res->length, 0);

    }

    if (bytes_sent == -1) {
        struct String *res = string_init(0, "HTTP/1.1 400 Bad Request\r\n\r\n");
        string_append(res, read_html_file("./public/400.html"));
        bytes_sent = send(conn_fd, res->data, res->length, 0);
        perror("Error with send()");
    }

    cleanup:
        free(conn_fd_ptr);
        free(req._buffer);
        free(req.headers);
        if (req.body != NULL) {
          string_free(req.body);
        }
        close(conn_fd);
        if (fp != NULL) {
          fclose(fp);
        }
        file->length = orig_file_len;
        file->data[orig_file_len] = '\0';

        return NULL;
}

void handle_connection(int conn_fd) {
    pthread_t new_thread;
    int *conn_fd_ptr = malloc(sizeof(int));
    if (conn_fd_ptr == NULL) {
        printf("Error allocating memory for connection file descriptor pointer");
        return;
    }
    *conn_fd_ptr = conn_fd;
    pthread_create(&new_thread, NULL, _handle_connection, conn_fd_ptr);
}

// Initialize new String or return NULL if error
struct String *string_init(size_t capacity, const char *const s) {

    struct String *str = malloc(sizeof(struct String));
    if (str == NULL) { return NULL; }
    if (capacity == 0) { capacity = STRING_INIT_CAPACITY; }
    const size_t s_len = (s == NULL) ? 0 : strlen(s);
    if (s_len >= capacity) {
      capacity = s_len + 1; // +1 for null terminator
    }

    str->data = malloc(sizeof(char) * capacity);
    if (str->data == NULL) {
        free(str);
        return NULL;
    }
    if (s != NULL) {
        memcpy(str->data, s, s_len);
    }

    str->capacity = capacity;
    str->length = s_len;
    str->data[s_len] = '\0';

    return str;
}

// Append C string at the end of a String
bool string_append(struct String *self, const char *const s) {

    assert(self != NULL);
    assert(s != NULL);

    const size_t slen = strlen(s);
    const size_t new_len = self->length + slen;

    if (new_len >= self->capacity) {
        size_t new_cap = self->capacity * 2;
        if (new_len >= new_cap) {
            new_cap = new_len + 1; // +1 for null terminator
        }
        char *new_data = realloc(self->data, new_cap);
        if (new_data == NULL) {
            return false;
        }
        self->data = new_data;
        self->capacity = new_cap;
    }

    memcpy(&self->data[self->length], s, slen);
    self->length = new_len;
    self->data[new_len] = '\0';

    return true;
}

void string_free(struct String *self) {
    assert(self != NULL);
    free(self->data);
    free(self);
}


// ************************** //
// ********** MAIN ********** //
// ************************** //
int main(int argc, char **argv) {

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = {htonl(INADDR_ANY)},
    };

    // Set directory for public files
    if (argc > 2) {
        file = string_init(0, argv[2]);
        if (argv[2][strlen(argv[2]) - 1] != '/') {
            string_append(file, "/");
        }
    } else {
        file = string_init(0, "./public/");
    }

    // Handle error with server file descriptor
    if (server_fd == -1) {
        printf("Error creating socket: %s\n", strerror(errno));
        goto cleanup;
    }

    // Allow reuse of address upon server restart by forcing socket to connect to port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &REUSE , sizeof(REUSE )) <
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
    if (listen(server_fd, CONNECTION_BACKLOG) != 0) {
        printf("Error listening for connection: %s\n", strerror(errno));
        goto cleanup;
    }

    printf("Connection ready\n");

    // Handle connection
    while (true) {

        int conn_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (conn_fd == -1) {
          printf("Error with accept(): %s \n", strerror(errno));
          continue;
        }

        handle_connection(conn_fd);
    }

    cleanup:
        if (server_fd != -1) {
            close(server_fd);
        }

        if (file != NULL) {
            string_free(file);
        }

    return 0;
}
