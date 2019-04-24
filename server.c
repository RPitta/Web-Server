/* server.c
A basic web server written in C that implements the HTTP protocol using the native network sockets library.
The web server also utilizes multithreading to process multiple requests concurrently.

The web server:
- Accepts TCP connections from web browsers
- Reads in the packets that the browsers send
- Detects the ends of requests by checking the contents of the packets
- Parses the request to determine the type of HTTP connection (persistent or non-persistent)
- Prepares and then sends a response to the web browser which then displays the content being requested. If the request is invalid, the response sends
  an appropriate error message

Usage:
	gcc -o server server.c
	./server <an available port number>
*/
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

bool writeDataToClient(int sckt, const void *data, int datalen)
{
    const char *pdata = (const char*) data;

    while (datalen > 0)
    {
        int numSent = send(sckt, pdata, datalen, 0);
        if (numSent <= 0){
            if (numSent == 0){
                printf("The client was not written to: disconnected\n");
            } else {
                perror("The client was not written to");
            }
            return false;
        }
        pdata += numSent;
        datalen -= numSent;
    }

    return true;
}

bool writeStrToClient(int sckt, const char *str)
{
    return writeDataToClient(sckt, str, strlen(str));
}

int get_filename_and_method(char *str, char **buf1, char **buf2)
{	
	char *request = str;
	char *status_line;
	char *url;
	char *token = strtok(request, "\r\n");
	status_line = token;

	*buf1 = strtok(status_line, " ");
	if (strcasecmp(*buf1, "GET") != 0) return -1;

	url = strtok(NULL, " ");
	if (strncmp(url, "/", strlen("/")) != 0) return -1;

	if (strlen(url) == 1) strcat(url, "index.html");
	if (url[strlen(url) - 1] == '/') strcat(url, "index.html");

	char *tmp = strdup(url);
	strcpy(url, "web");		// Directory where webpage files are stored 
	strcat(url, tmp);
	*buf2 = url;

	free(tmp);

	return 0;
}

int get_connection_type(char *str, char **buf)
{	
	char *req = str;
	char *token = strtok(req, "\r\n");
	char *connection;

	while (token != NULL)
	{	

		if (strncmp(token, "Connection:", 11) == 0)
		{	
			connection = token;
			strtok(connection, " ");
			if (strcasecmp(strtok(NULL, " "), "Keep-Alive") == 0)
			{	
				*buf = "Connection: keep-alive\r\n\r\n";
				return 0;
			}
		}
			
		token = strtok(NULL, "\r\n");
	}

	*buf = "Connection: close\r\n\r\n";
	return 0;
}

void *connection_handler (void *sockfd)
{
    int sock = *(int*)sockfd;
    char *buffer, *method, *filename, *connection_type, *content_type;
    int bufsize = 2048;

    const char *HTTP_404_CONTENT = "<html><head><title>404 Not "
	"Found</title></head><body><h1>404 Not Found</h1>The requested "
	"resource could not be found but may be available again in the "
	"future.<div style=\"color: #eeeeee; font-size: 8pt;\">Actually, "
	"it probably won't ever be available unless this is showing up "
	"because of a bug in your program. :(</div></html>";

	const char *HTTP_501_CONTENT = "<html><head><title>501 Not "
	"Implemented</title></head><body><h1>501 Not Implemented</h1>The "
	"server either does not recognise the request method, or it lacks "
	"the ability to fulfill the request.</body></html>";

    buffer = (char*) malloc(bufsize);    
    if (!buffer)
    {
        printf("The receive buffer was not allocated\n");
        exit(1);    
    }

    while (1)
    {
        int numRead = recv(sock, buffer, bufsize, 0);
        if (numRead < 1){
            if (numRead == 0){
                printf("The client was not read from: disconnected\n");
                break;
            } else {
                perror("The client was not read from");
                break;
            }
            close(sock);
            continue;
        }
        printf("%.*s\n", numRead, buffer);

        // Extract information from request header
        get_connection_type(buffer, &connection_type);
        if (get_filename_and_method(buffer, &method, &filename) == -1)
        {
        	char clen[40];
        	writeStrToClient(sock, "HTTP/1.1 501 Not Implemented\r\n");
        	sprintf(clen, "Content-length: %zu\r\n", strlen(HTTP_501_CONTENT));
        	writeStrToClient(sock, clen);
        	writeStrToClient(sock, "Content-Type: text/html\r\n");
        	writeStrToClient(sock, connection_type);
        	writeStrToClient(sock, HTTP_501_CONTENT);
        }
        else
        {

	        // Open and read file
	        long fsize;
		    FILE *fp = fopen(filename, "rb");
		    if (!fp){
		        perror("The file was not opened");
		        char clen[40];
	        	writeStrToClient(sock, "HTTP/1.1 404 Not Found\r\n");
	        	sprintf(clen, "Content-length: %zu\r\n", strlen(HTTP_404_CONTENT));
	        	writeStrToClient(sock, clen);
	        	writeStrToClient(sock, "Content-Type: text/html\r\n");
	        	writeStrToClient(sock, connection_type);
	        	writeStrToClient(sock, HTTP_404_CONTENT);

	        	if (strcmp(connection_type, "Connection: close\r\n\r\n") == 0)
        			break;
        		
		        continue;    
		    }

		    printf("The file was opened\n");

		    if (fseek(fp, 0, SEEK_END) == -1){
		        perror("The file was not seeked");
		        exit(1);
		    }

		    fsize = ftell(fp);
		    if (fsize == -1) {
		        perror("The file size was not retrieved");
		        exit(1);
		    }
		    rewind(fp);

		    char *msg = (char*) malloc(fsize);
		    if (!msg){
		        perror("The file buffer was not allocated\n");
		        exit(1);
		    }

		    if (fread(msg, fsize, 1, fp) != 1){
		        perror("The file was not read\n");
		        exit(1);
		    }
		    fclose(fp);

		    // Get extension of filename
			char *ext = strrchr(filename, '.');
			if (ext != NULL)
				ext++;
			if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0)
				content_type = "Content-Type: text/html\r\n";
			else if (strcmp(ext, "css") == 0)
				content_type = "Content-Type: text/css\r\n";
			else if (strcmp(ext, "jpg") == 0)
				content_type = "Content-Type: image/jpeg\r\n";
			else if (strcmp(ext, "png") == 0)
				content_type = "Content-Type: image/png\r\n";
			else if (strcmp(ext, "gif") == 0)
				content_type = "Content-Type: image/gif\r\n";
			else
				content_type = "Content-Type: text/plain\r\n";


			// Here we begin writing data to the socket
	        if (!writeStrToClient(sock, "HTTP/1.1 200 OK\r\n"))
	        {
	            close(sock);
	            continue;
	        }

	        char clen[40];
	        sprintf(clen, "Content-length: %ld\r\n", fsize);
	        if (!writeStrToClient(sock, clen))
	        {
	            close(sock);
	            continue;
	        }

	        if (!writeStrToClient(sock, content_type))
	        {
	            close(sock);
	            continue;
	        }

	        if (!writeStrToClient(sock, connection_type) == -1)
	        {
	            close(sock);
	            continue;
	        }

	        if (!writeDataToClient(sock, msg, fsize))
	        {
	            close(sock);
	            continue;
	        }

	        printf("The file was sent successfully\n");
        }

        // Close the connection if the connection field in the 
        // request header indicates to do so
        if (strcmp(connection_type, "Connection: close\r\n\r\n") == 0)
        	break;
	}

	close(sock);
	pthread_exit(0);
}

int main(int argc, char *argv[])
{
    int create_socket, new_socket;    
    struct sockaddr_in address;    
    socklen_t addrlen;    
    char *ptr;

    if (argc != 2)
	{
		printf("Usage: %s <port number>\n", argv[0]);
		exit(0);
	}

    create_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (create_socket == -1){    
        perror("The socket was not created");    
        exit(1);    
    }

    printf("The socket was created\n");

    const unsigned short port = (unsigned short) strtol(argv[1], &ptr, 10);

    memset(&address, 0, sizeof(address));    
    address.sin_family = AF_INET;    
    address.sin_addr.s_addr = INADDR_ANY;    
    address.sin_port = htons(port);    

    if (bind(create_socket, (struct sockaddr *) &address, sizeof(address)) == -1)
    {    
        printf("The socket was not bound because that port is not available\n");    
        exit(1);    
    }

    printf("The socket is bound\n");    

    if (listen(create_socket, 10) == -1)
    {
        perror("The socket was not opened for listening");    
        exit(1);    
    }    

    printf("The socket is listening\n");

    while (1) 
    {    

        addrlen = sizeof(address);
        pthread_t tid;
        new_socket = accept(create_socket, (struct sockaddr *) &address, &addrlen);

        if (new_socket == -1) 
        {    
            perror("A client was not accepted");    
            exit(1);    
        }    

        if (pthread_create(&tid, NULL, connection_handler, (void *)&new_socket) < 0)
        {
        	perror("Could not create thread");
        	return 1;
        }

        sleep(1);
   }

   if (new_socket < 0)
   {
   	perror("accept failed");
   	return 1;
   }    

   close(create_socket);
   printf("Socket was closed\n");
   return 0;    
}