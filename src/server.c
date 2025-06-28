#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>


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


// HTTP methods
enum HTTPMethodType {
  HTTP_UNKNOWN = 0,
  HTTP_GET,
  HTTP_POST,
  HTTP_PUT,
  HTTP_PATCH,
  HTTP_DELETE,
};

struct HTTPMethod {
  const char *const str;
  enum HTTPMethodType typ;
};

struct HTTPMethod KNOWN_HTTP_METHODS[] = {
    {.str = "GET", .typ = HTTP_GET},
    {.str = "POST", .typ = HTTP_POST},
    {.str = "PUT", .typ = HTTP_PUT},
    {.str = "PATCH", .typ = HTTP_PATCH},
    {.str = "DELETE", .typ = HTTP_DELETE},
};

size_t KNOWN_HTTP_METHODS_LEN =
    sizeof(KNOWN_HTTP_METHODS) / sizeof(KNOWN_HTTP_METHODS[0]);

struct HTTPHeader {
  char *key;
  char *value;
};

struct HTTPRequest {
  enum HTTPMethodType method;
  char *path;

  // Headers
  struct HTTPHeader *headers;
  size_t headers_len;

  // Request buffer
  char *_buffer;
  size_t _buffer_len;

  struct BString *body;
};


int main() {
    printf("Hello, World!\n");
    return 0;
}
