#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "rdt.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("uso: %s <porta_servidor> \n", argv[0]);
        return 0;
    }

    int s, i;
    struct sockaddr_in saddr, caddr;
    socklen_t addrlen = sizeof(caddr);

    // Meu UDP socket
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (s < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configuro o endereço do servidor
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_port = htons(atoi(argv[1]));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;

    // Associo o socket ao endereço do servidor
    if (bind(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        perror("bind");
        close(s);
        exit(EXIT_FAILURE);
    }

    printf("Servidor aguardando mensagens na porta %s...\n\n", argv[1]);

    // Buffer para receber a mensagem
    int msg;
    memset(&msg, 0, sizeof(msg));

    int val;

    for(i = 0; i < 100; i++) {
        // gero valores entre 1 e 10
        val = rand() % 10 + 1;

        printf("\n%d segundos passados | ", val);
        sleep(val);
        
        if (rdt_recv_static(s, &msg, sizeof(msg), &caddr) < 0)
        {
            printf("Mensagem nao recebida\n");
            perror("rdt_recv");
            close(s);
            exit(EXIT_FAILURE);
        }
        printf("Mensagem recebida: %d\n", msg);
    }

    close(s);
    return 0;
}