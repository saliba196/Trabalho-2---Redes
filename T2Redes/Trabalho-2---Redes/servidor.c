#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#include "rdt.h"

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("uso: %s <ip_servidor> <porta_servidor> \n", argv[0]);
        return 0;
    }

    int s, i;
    struct sockaddr_in saddr;

    // Crio o socket UDP
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configuro o endereço do servidor
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_port = htons(atoi(argv[2]));
    saddr.sin_family = AF_INET;
    inet_aton(argv[1], &saddr.sin_addr);

    // Mensagem a ser enviada
    int msg = 30012025;
    printf("Enviando mensagem: %d\n", msg);

    for (i = 0; i < 100; i++) {
        // Envio da mensagem usando RDT
        if (rdt_send_static(s, &msg, sizeof(msg), &saddr) < 0)
        {
            printf("Mensagem não enviada!\n\n");
            perror("rdt_send");
            close(s);
            exit(EXIT_FAILURE);
        }
        else
            printf("Mensagem enviada com sucesso!\n\n");
    }

    close(s);
    return 0;
}