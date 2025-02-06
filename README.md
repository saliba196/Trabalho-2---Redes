#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "rdt.h"  // Certifique-se de que este cabeçalho define corretamente rdt_recv_static

#define SERVER_PORT 5000
#define BUFFER_SIZE 1024

int main() {
    int s;
    struct sockaddr_in server_addr, caddr;
    socklen_t addrlen = sizeof(caddr);
    char msg[BUFFER_SIZE];

    // Criando o socket
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configurando o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Enviando uma mensagem ao servidor
    strcpy(msg, "Olá, servidor!");
    if (sendto(s, msg, strlen(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao enviar mensagem");
        close(s);
        exit(EXIT_FAILURE);
    }

    printf("Mensagem enviada ao servidor.\n");

    // Recebendo resposta do servidor usando a função rdt_recv_static
    memset(msg, 0, BUFFER_SIZE);
    if (rdt_recv_static(s, &msg, sizeof(msg), &caddr) < 0) {
        perror("Erro ao receber mensagem");
        close(s);
        exit(EXIT_FAILURE);
    }

    printf("Resposta do servidor: %s\n", msg);

    // Fechando o socket
    close(s);
    return 0;
}
