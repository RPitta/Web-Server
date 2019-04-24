# Web Server
A basic web server written in C that implements the HTTP protocol using the native network sockets library.
The web server also utilizes multithreading to process multiple requests concurrently.

***

The web server:
- Accepts TCP connections from web browsers
- Reads in the packets that the browsers send
- Detects the ends of requests by checking the contents of the packets
- Parses the request to determine the type of HTTP connection (persistent or non-persistent)
- Prepares and then sends a response to the web browser which then displays the content being requested. If the request is invalid, the response sends
  an appropriate error message

***

## Usage
### Compiling
`gcc -o server server.c`

### Running
`./server <an available port number>`