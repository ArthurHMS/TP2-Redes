#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>

char *ROOT;  // Diretório raiz para os arquivos

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void send_response(int newsockfd, const char *status, const char *content_type, const char *body) {
    char response[4096];
    sprintf(response, "HTTP/1.1 %s\r\nContent-Length: %ld\r\nContent-Type: %s\r\nConnection: Closed\r\n\r\n%s",
            status, strlen(body), content_type, body);
    write(newsockfd, response, strlen(response));
}

void handle_request(int newsockfd) {
    char buffer[2048];
    int n = read(newsockfd, buffer, sizeof(buffer) - 1);
    if (n < 0) error("ERROR reading from socket");
    buffer[n] = '\0';

    // Parse the request line
    char method[16], path[256], protocol[16];
    sscanf(buffer, "%s %s %s", method, path, protocol);

    // Only handle GET requests
    if (strcmp(method, "GET") != 0) {
        send_response(newsockfd, "405 Method Not Allowed", "text/plain", "Method Not Allowed");
        return;
    }

    // Construct the file path
    char filepath[512];
    if (strcmp(path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", ROOT);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", ROOT, path);
    }

    // Check if the path is a directory
    struct stat path_stat;
    stat(filepath, &path_stat);
    if (S_ISDIR(path_stat.st_mode)) {
        send_response(newsockfd, "403 Forbidden", "text/plain", "Forbidden: Is a directory");
        return;
    }

    // Open the file
    int filefd = open(filepath, O_RDONLY);
    if (filefd < 0) {
        perror("ERROR opening file");
        send_response(newsockfd, "404 Not Found", "text/plain", "File Not Found");
        return;
    }

    // Get the file size
    struct stat filestat;
    if (fstat(filefd, &filestat) < 0) {
        perror("ERROR getting file size");
        send_response(newsockfd, "500 Internal Server Error", "text/plain", "Internal Server Error");
        close(filefd);
        return;
    }

    // Read the file content
    char *filecontent = malloc(filestat.st_size);
    if (filecontent == NULL) {
        perror("ERROR allocating memory");
        send_response(newsockfd, "500 Internal Server Error", "text/plain", "Internal Server Error");
        close(filefd);
        return;
    }

    if (read(filefd, filecontent, filestat.st_size) < 0) {
        perror("ERROR reading file");
        send_response(newsockfd, "500 Internal Server Error", "text/plain", "Internal Server Error");
        free(filecontent);
        close(filefd);
        return;
    }
    close(filefd);

    // Send the response
    send_response(newsockfd, "200 OK", "text/html", filecontent);
    free(filecontent);
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <root_directory>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);
    ROOT = argv[2];

    // Ensure ROOT is a directory, not a file
    struct stat root_stat;
    if (stat(ROOT, &root_stat) < 0 || !S_ISDIR(root_stat.st_mode)) {
        fprintf(stderr, "ERROR: Root directory is not valid\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) error("ERROR opening socket");
    printf("Socket created successfully\n");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");
    printf("Binding successful\n");

    listen(sockfd, 5);
    printf("Listening on port %d\n", portno);
    clilen = sizeof(cli_addr);

    printf("Server started on port %d with root directory %s\n", portno, ROOT);

    while (1) {
        printf("Waiting for a connection...\n");
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");

        printf("Accepted connection from client\n");

        handle_request(newsockfd);
        close(newsockfd);
        printf("Connection closed\n");
    }

    close(sockfd);
    return 0;
}
