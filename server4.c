#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BUFFER_SIZE 1024

char *ROOT;  // Diretório raiz para os arquivos

void send_response(int client_fd, const char *status, const char *content_type, const char *body) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n"
             "%s",
             status, content_type, strlen(body), body);
    send(client_fd, response, strlen(response), 0);
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read < 0) {
        perror("ERROR reading from socket");
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    // Parse HTTP request
    char method[16], path[256];
    sscanf(buffer, "%s %s", method, path);

    // Construct file path
    char filepath[512];
    if (strcmp(path, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", ROOT);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", ROOT, path);
    }

    printf("Requested file: %s\n", filepath);  // Debugging message

    // Check if the path is a directory
    struct stat path_stat;
    if (stat(filepath, &path_stat) < 0) {
        perror("ERROR stat file");
        send_response(client_fd, "404 Not Found", "text/plain", "File Not Found");
        close(client_fd);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        send_response(client_fd, "403 Forbidden", "text/plain", "Forbidden: Is a directory");
        close(client_fd);
        return;
    }

    // Open the file
    int filefd = open(filepath, O_RDONLY);
    if (filefd < 0) {
        perror("ERROR opening file");
        send_response(client_fd, "404 Not Found", "text/plain", "File Not Found");
        close(client_fd);
        return;
    }

    // Get the file size
    struct stat filestat;
    if (fstat(filefd, &filestat) < 0) {
        perror("ERROR getting file size");
        send_response(client_fd, "500 Internal Server Error", "text/plain", "Internal Server Error");
        close(filefd);
        close(client_fd);
        return;
    }

    // Read the file content
    char *file_content = malloc(filestat.st_size);
    if (read(filefd, file_content, filestat.st_size) < 0) {
        perror("ERROR reading file");
        send_response(client_fd, "500 Internal Server Error", "text/plain", "Internal Server Error");
        free(file_content);
        close(filefd);
        close(client_fd);
        return;
    }

    // Send the response
    send_response(client_fd, "200 OK", "text/html", file_content);

    // Clean up
    free(file_content);
    close(filefd);
    close(client_fd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <port> <root_directory>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    ROOT = argv[2];

    // Ensure ROOT is a directory, not a file
    struct stat root_stat;
    if (stat(ROOT, &root_stat) < 0 || !S_ISDIR(root_stat.st_mode)) {
        fprintf(stderr, "ERROR: Root directory is not valid\n");
        exit(1);
    }

    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    fd_set read_fds, master_fds;
    int fd_max;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR opening socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("ERROR setting socket options");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERROR binding socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("ERROR listening on socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Initialize fd sets
    FD_ZERO(&master_fds);
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &master_fds);
    fd_max = server_fd;

    printf("Server is listening on port %d with root directory %s\n", port, ROOT);

    while (1) {
        read_fds = master_fds;
        if (select(fd_max + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("ERROR in select");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i <= fd_max; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
                    // New connection
                    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
                        perror("ERROR accepting connection");
                        continue;
                    }
                    FD_SET(client_fd, &master_fds);
                    if (client_fd > fd_max) {
                        fd_max = client_fd;
                    }
                    printf("New connection from %s on socket %d\n", inet_ntoa(client_addr.sin_addr), client_fd);
                } else {
                    // Handle client
                    handle_client(i);
                    FD_CLR(i, &master_fds);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}