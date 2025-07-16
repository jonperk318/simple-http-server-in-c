# Simple HTTP Server in C

This is a simple multi-threaded HTTP server written in C that only uses the C Standard Library.
It listens on port 3000 default.


## Setup

1. Clone the repository
    ```bash
    git clone https://github.com/jonperk318/simple-http-server-in-c
    ```
2. Compile using Makefile
    ```bash
    make
    ```
3. Test the server with `curl`
    ```bash
    curl -v --http0.9 localhost:3000/
    ```
    or
    ```bash
    curl -v localhost:3000/echo/test
    ```
4. Stop the server with `Ctrl+C`
