#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <dirent.h>

#define BACKLOG (10)

char *request_str = "HTTP/1.0 200 OK\r\n"
                    "Content-type: %s; charset=UTF-8\r\n\r\n";

char *index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
                  "<title>Directory listing for %s</title>"
                  "<body>"
                  "<h2>Directory listing for %s</h2><hr><ul>";

char *index_body = "<li><a href=\"%s\">%s</a>";
char *index_ftr = "</ul><hr></body></html>";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X" 
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request 
 * 
 * Does not modify the given request string. 
 * The returned resource should be free'd by the caller function. 
 */
char *parseRequest(char *request)
{
    //assume file paths are no more than 256 bytes + 1 for null.
    char *buffer = malloc(sizeof(char) * 257);
    memset(buffer, 0, 257);

    if (fnmatch("GET * HTTP/1.*", request, 0))
        return 0;

    sscanf(request, "GET %s HTTP/1.", buffer);
    return buffer;
}

void fileType(char *requested_file, int client_fd)
{
    //if the file is .html
    if (strstr(requested_file, ".html"))
    {
        char *request_str = "HTTP/1.0 200 OK\r\n"
                            "Content-type: text/html; charset=UTF-8\r\n\r\n";
        send(client_fd, request_str, strlen(request_str), 0);
    }
    //if the file is a pdf
    else if (strstr(requested_file, ".pdf"))
    {
        char *request_str = "HTTP/1.0 200 OK\r\n"
                            "Content-type: application/pdf; charset=UTF-8\r\n\r\n";
        send(client_fd, request_str, strlen(request_str), 0);
    }
    //if it is a gif file
    else if (strstr(requested_file, ".gif"))
    {
        char *request_str = "HTTP/1.0 200 OK\r\n"
                            "Content-type: image/gif; charset=UTF-8\r\n\r\n";
        send(client_fd, request_str, strlen(request_str), 0);
    }
    //if it is jpeg or jpg since they are the same file formats
    else if (strstr(requested_file, ".jpeg") || strstr(requested_file, ".jpg"))
    {
        char *request_str = "HTTP/1.0 200 OK\r\n"
                            "Content-type: image/jpeg; charset=UTF-8\r\n\r\n";
        send(client_fd, request_str, strlen(request_str), 0);
    }
    //if it is png
    else if (strstr(requested_file, ".png"))
    {
        char *request_str = "HTTP/1.0 200 OK\r\n"
                            "Content-type: image/png; charset=UTF-8\r\n\r\n";
        send(client_fd, request_str, strlen(request_str), 0);
    }
    //if it is txt
    else if (strstr(requested_file, ".txt"))
    {
        char *request_str = "HTTP/1.0 200 OK\r\n"
                            "Content-type: text/plain; charset=UTF-8\r\n\r\n";
        send(client_fd, request_str, strlen(request_str), 0);
    }
}

void serve_404_request(int client_fd)
{
    char *response_str = "HTTP/1.0 404 NOT FOUND\r\n"
                         "Content-type: text/html; charset=UTF-8\r\n\r\n";

    send(client_fd, response_str, strlen(response_str), 0);

    char *page_404 = "<h1> ERROR: THIS PAGE DOES NOT EXIST!!</h1>";
    if (send(client_fd, page_404, strlen(page_404), 0) < 0)
    {
        printf("sending of 404 page failed\n");
        return;
    }
}

void serve_file(char *filename, int client_fd)
{

    char send_buf[4096];
    memset(send_buf, 0, 4096);

    int read_fd = open(filename, 0, 0);

    //if the file does not exists with in the directory
    if (read_fd < 0)
    {
        printf("404 Error!\n");
        serve_404_request(client_fd);
        return;
    }

    fileType(filename, client_fd);
    int bytes_read = read(read_fd, send_buf, 4096);

    while (bytes_read > 0)
    {
        int bytes_sent = send(client_fd, send_buf, bytes_read, 0);

        if (bytes_sent < 0 || bytes_sent != bytes_read)
        {
            printf("bytes sent are 0\n");
        }

        bytes_read = read(read_fd, send_buf, 4096);
        if (bytes_read < 0)
        {
            printf("bytes read are 0\n");
        }
    }

    close(read_fd);
}

void serve_newfile(char *dirname, int client_fd)
{

    // Open the directory
    struct dirent *underlying_file = NULL;
    DIR *path = opendir(dirname);

    if (path == 0)
    {
        printf("no directory to open\n");
        return;
    }

    char send_buf[4096];
    memset(send_buf, 0, 4096);

    sprintf(send_buf, request_str, "text/html");
    send(client_fd, send_buf, strlen(send_buf), 0);
    memset(send_buf, 0, 4096);
    sprintf(send_buf, index_hdr, dirname, dirname);
    send(client_fd, send_buf, strlen(send_buf), 0);

    while ((underlying_file = readdir(path)) != NULL)
    {
        memset(send_buf, 0, 4096);
        sprintf(send_buf, index_body, underlying_file->d_name, underlying_file->d_name);
        send(client_fd, send_buf, strlen(send_buf), 0);
    }
    // now send the created index.html file
    send(client_fd, index_ftr, strlen(index_ftr), 0);
    closedir(path);
}

// open the directory and check if intex.html exists or not
int has_file(char *dirname)
{
    DIR *path = opendir(dirname);
    struct dirent *underlying_file = NULL;

    if (path == 0)
    {
        printf("there is no path to display\n");
        return 0;
    }

    while ((underlying_file = readdir(path)) != NULL)
    {
        // check if the directory has index.html
        if (strcmp(underlying_file->d_name, "index.html") == 0)
        {
            closedir(path);
            return 1;
        }
    }

    closedir(path);
    return 0;
}

// check if the directory exist
// if so then check if index.html exist
// if not then create index.html (which is done by the function serve_newfile)
void serve_directory(char *dirname, int client_fd)
{
    //directory exists -> check if the directory has index.html file
    if (has_file(dirname))
    {
        printf("index.html file exists\n");
        char *newfile = malloc(strlen(dirname) + 12);
        sprintf(newfile, "%s/%s", dirname, "index.html");
        serve_file(newfile, client_fd);
        return;
    }
    //directory exists-> does not have index.html file
    printf("creating an index.html\n");
    serve_newfile(dirname, client_fd);
}

void serve_request(int client_fd)
{

    int file_offset = 0;
    char client_buf[4096];
    memset(client_buf, 0, 4096);
    char filename[258];
    memset(filename, 0, 258);
    struct stat file_stat;

    while (1)
    {
        file_offset += recv(client_fd, &client_buf[file_offset], 4096 - file_offset, 0);
        if (strstr(client_buf, "\r\n\r\n"))
            break;
    }

    // take requested_file, add a . to beginning, open that file
    char *requested_file = parseRequest(client_buf);
    filename[0] = '.';
    strncpy(&filename[1], requested_file, 257);

    //check if the file exists or not
    if (stat(filename, &file_stat) < 0)
    {
        serve_404_request(client_fd);
    }

    //check if the client request is a directory
    if (S_ISDIR(file_stat.st_mode))
    {
        printf("%s is a directory!\n", filename);
        serve_directory(filename, client_fd);
    }
    //check if request is a file
    else
    {
        printf("%s is a file!\n", filename);
        serve_file(filename, client_fd);
    }

    close(client_fd);
    return;
}

/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char **argv)
{

    /* For checking return values. */
    int retval;

    chdir(argv[2]);

    /* Read the port number from the first command line argument. */
    int port = atoi(argv[1]);

    /* Create a socket to which clients will connect. */
    int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        perror("Creating socket failed");
        exit(1);
    }

    /* A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options. */
    int reuse_true = 1;
    retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));
    if (retval < 0)
    {
        perror("Setting socket option failed");
        exit(1);
    }

    /* Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here. */
    struct sockaddr_in6 addr; // internet socket address data structure
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port); // byte order is significant
    addr.sin6_addr = in6addr_any; // listen to all interfaces

    /* As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above. */
    retval = bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    if (retval < 0)
    {
        perror("Error binding to port");
        exit(1);
    }

    /* Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections.  This effectively
     * activates the server socket.  BACKLOG (#defined above) tells the OS how
     * much space to reserve for incoming connections that have not yet been
     * accepted. */
    retval = listen(server_sock, BACKLOG);
    if (retval < 0)
    {
        perror("Error listening for connections");
        exit(1);
    }

    while (1)
    {
        /* Declare a socket for the client connection. */
        int sock;

        /* Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from. */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr);

        /* Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         * */
        sock = accept(server_sock, (struct sockaddr *)&remote_addr, &socklen);
        if (sock < 0)
        {
            perror("Error accepting connection");
            exit(1);
        }

        /* At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). */

        /* ALWAYS check the return value of send().  Also, don't hardcode
         * values.  This is just an example.  Do as I say, not as I do, etc. */
        
        //multi threading
        pthread_t *thread = malloc(sizeof(pthread_t));
        retval = pthread_create(thread, NULL, serve_request, (void *)sock);
        if (retval)
        {
            printf("pthread_create() failed\n");
            exit(1);
        }
    }

    close(server_sock);
}
