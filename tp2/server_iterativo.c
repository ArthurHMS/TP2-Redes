#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

char *ROOT = "arquivos";  // Diretório raiz para os arquivos (não utilizado neste exemplo)

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    char line[2048];
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    // Verificar se o número de argumentos está correto
    if (argc < 2) {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    // Criar o socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    // Zerar a estrutura do servidor
    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    // Definir a porta
    portno = atoi(argv[1]);

    // Configurar a estrutura do servidor
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;  // Qualquer endereço IP
    serv_addr.sin_port = htons(portno);

    // Associar o socket a um endereço e porta
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    // Começar a escutar as conexões
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    // Laço principal de aceitação e processamento de conexões
    while (1) {
        // Aceitar uma nova conexão
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) 
            error("ERROR on accept");

        // Processar a requisição do cliente
        int count = 0;
        while (1) {
            n = read(newsockfd, &line[count], 1);
            if (n < 0) 
                error("ERROR reading from socket");

            // Verificar se recebemos uma linha completa (terminada com \r\n)
            if (count > 0 && line[count-1] == '\r' && line[count] == '\n') {
                line[count+1] = '\0';

                // Verificar se a linha é uma requisição HTTP básica
                if (strlen(line) == 2 && line[0] == '\r' && line[1] == '\n') {
                    // Preparar a resposta HTTP
                    char *response = "HTTP/1.1 200 OK\r\n"
                                     "Content-Length: 88\r\n"
                                     "Content-Type: texsockfd = socket(AF_INET, SOCK_STREAM, 0);t/html\r\n"
                                     "Connection: Closed\r\n"
                                     "\r\n"
                                     "<html>"
                                     "<body>"
                                     "<h1>Hello, World!</h1>"
                                     "</body>"
                                     "</html>";

                    // Enviar a resposta ao cliente
                    write(newsockfd, response, strlen(response));

                    // Fechar a conexão com o cliente após responder
                    close(newsockfd);
                    break;  // Sai do loop de leitura para aceitar uma nova conexão
                }

                count = 0;  // Resetar o contador de leitura
            }
            count++;
        }
    }

    // Fechar o socket do servidor após terminar
    close(sockfd);
    return 0;
}
