#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    char buffer[2048];
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) 
            error("ERROR on accept");

        memset(buffer, 0, sizeof(buffer));

        // Ler a requisição HTTP
        n = read(newsockfd, buffer, sizeof(buffer) - 1);
        if (n < 0) 
            error("ERROR reading from socket");

        printf("Request:\n%s\n", buffer);

        // Verificar se é uma requisição HTTP básica
        if (strstr(buffer, "GET / HTTP/1.1") != NULL) {
            // Corpo da resposta
            char *body = "<html><body><h1>Hello, World!</h1></body></html>";
            int body_length = strlen(body);

            // Cabeçalho da resposta
            char header[256];
            snprintf(header, sizeof(header),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Length: %d\r\n"
                     "Content-Type: text/html\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     body_length);

            // Enviar cabeçalho e corpo
            n = write(newsockfd, header, strlen(header));
            if (n < 0) 
                error("ERROR writing header to socket");

            n = write(newsockfd, body, body_length);
            if (n < 0) 
                error("ERROR writing body to socket");
        } else {
            // Responder com "404 Not Found"
            char *body = "404 Not Found";
            int body_length = strlen(body);

            char header[256];
            snprintf(header, sizeof(header),
                     "HTTP/1.1 404 Not Found\r\n"
                     "Content-Length: %d\r\n"
                     "Content-Type: text/plain\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     body_length);

            n = write(newsockfd, header, strlen(header));
            if (n < 0) 
                error("ERROR writing header to socket");

            n = write(newsockfd, body, body_length);
            if (n < 0) 
                error("ERROR writing body to socket");
        }

        // Fechar o socket do cliente
        close(newsockfd);
    }

    close(sockfd);
    return 0;
}
