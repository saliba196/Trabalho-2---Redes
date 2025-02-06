#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include "rdt.h"

// Podemos injetar a simulacao de bit errors nos pacotes 
// para testar o protocolo
int biterror_inject = FALSE;

int timeout = INITIAL_TIMEOUT; // SERA QUE PRECISAMOS MESMO DE INITIAL_TIMEOUT??????????????????????????

// Usamos o valor inicial de seqnum dos pacotes para sabermos se o pacote eh
// novo ou duplicado, isso nos ajuda a seguir a ordenacao de pacotes a serem enviados
hseq_t snd_base = 1;
hseq_t rcv_base = 1;

// Para a janela de transmissao usamos um window_size de tamanho N
int window_size = WINDOW_SIZE;

/*
    Costurar cap 3.3 sobre no connection state no UDP <============================================
    
    Precisamos de um buffer no lado remetente e outro
    no lado receptor do canal, dessa forma, com os
    sequence numbers nos temos controle do estado 
    da conexao na camada de aplicacao mesmo sem um canal confiavel

    O receiver sequence repeater ira reconhecer um pacote recebido
    independente de ter chego em ordem ou nao. 
    
    Nesse caso, esses pacotes sao armazenados em buffers ate que
    todos os pacotes faltantes cheguem < ===================================
*/
pkt rcv_buff[MAX_WINDOW_SIZE];
pkt snd_buff[MAX_WINDOW_SIZE]; // MANTENHO PACOTES
int rcv_ack[MAX_WINDOW_SIZE]; 
int snd_ack[MAX_WINDOW_SIZE]; // MANTENHO STATUS DO ACK DE CADA PACOTE

/*
    ----------------------- Propriedades de um RDT protocol -----------------------

    Checksum: É inerente a qualquer protocolo da camada de transporte (TCP ou UDP). Geralmente
    nós somamos os campos de 16 bits do cabecalho do segmento, fazemos seu complemento e salvamos
    no campo checksum. No lado do destinatário é feito o complemento do campo de checksum e sua soma
    com o mesmo, que deve ser considerado corrompido por qualquer número diferente de um binário de 16 bits 1's.
*/
unsigned short checksum(unsigned short *buf, int nbytes)
{
    register long sum = 0;

    // se o segmento tem mais de 1 byte provavelmente sera um campo de 16 bits
    while (nbytes > 1)
    {
        sum += *(buf++);
        nbytes -= 2; // estamos somando dois campos do header do segmento
    }

    // se so temos um byte, nos consideramos que estamos somando um numero de 2 bytes,
    // esse e o caso de inicializacao do checksum
    if (nbytes == 1)
        sum += *(unsigned short *)buf;

    // faço deslocamento para a direita sempre que tiver overflow
    // na soma de dois binarios de 16 bits
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    // faco o complemento do binario resultante
    return (unsigned short)~sum;
}

// No receptor chamo o mecanismo checksum para verificar a integridade
// e defino como corrupto ou nao a depender do resultado
int iscorrupted(pkt *pr)
{
    pkt pl = *pr;
    pl.h.csum = 0; // inicialmente zero

    unsigned short csuml = checksum((void *)&pl, pl.h.pkt_size);

    // Retorno TRUE ou FALSE se o valor de checksum calculado
    // no destinatario eh igual ao checksum salvo no segmento
    return (csuml != pr->h.csum);
}

// Função para criar um pacote independente do lado do canal  nao confiavel
int make_pkt(pkt *p, htype_t type, hseq_t seqnum, void *msg, int msg_len)
{
    if (msg_len > MAX_MSG_LEN)
    {
        printf("make_pkt: tamanho da msg (%d) maior que limite (%d).\n", msg_len, MAX_MSG_LEN);
        return ERROR;
    }

    p->h.pkt_size = sizeof(hdr);
    p->h.csum = 0;        // inicializo checksum
    p->h.pkt_type = type; // eh DATA ou ACK?
    p->h.pkt_seq = seqnum;

    if (msg_len > 0)
    {
        p->h.pkt_size += msg_len;

        // Preencho um bloco de memoria de tamanho MAX_MSG_LEN com o valor 0
        memset(p->msg, 0, MAX_MSG_LEN);

        // Copio bytes de um espaco de memoria para outro
        // Ou seja, salvo a mensagem no campo do pacote
        memcpy(p->msg, msg, msg_len);
    }

    // Crio o campo de checksum desse pacote/segmento
    p->h.csum = checksum((unsigned short *)p, p->h.pkt_size);

    return SUCCESS;
}

// Verifico se o pacote eh de ACK, NAK ou DATA seguindo a sequenciacao correta
// Se for ACK retorno TRUE
int has_ackseq(pkt *p, hseq_t seqnum)
{
    return (p->h.pkt_type == PKT_ACK && p->h.pkt_seq == seqnum);
}

// Verifico se o segmento eh de ACK, NAK ou DATA seguindo a sequenciacao correta
// Se for DATA retorno TRUE 
// Acho que o nome da funcao nao eh apropriado pra o que ela faz
int is_the_pkt_duplicated(pkt *p, hseq_t seqnum)
{
    return (p->h.pkt_type == PKT_ACK && p->h.pkt_seq >= seqnum);
}

// ----------------------- Troca de mensagens pelo protocolo RDT -----------------------
/*
    O protocolo RDT deve ser implementado sobre uma camada com protocolo não confiável,
    esse é o motivo de usarmos sockets UDP.
    O canal de comunicação é feito pelo protocolo IP como um canal não confiável.
    Essa função será chamada pela aplicação quando for necessário enviar um pacote para outro host,
    no caso, inicialmente a aplicação chama rdt_send() que, na camada de transporte, através do protocolo
    udp chama a função udt_send() que inicia o canal não confiável na camada de rede.

    O protocolo RDT 2 implementado é o mesmo que o RDT ARQ, que segue os princípios de:
    - Checksum: Verificação de bit errors nos pacotes de ACK, NAK e DATA;
    - Feedback do Processo Receptor: Os pacotes de ACK ou NAK são enviados para fazer reconhecimento de pacotes na comunicação;
    - Retransmissão de pacote: O reenvio de pacotes perdidos (detecção via timeout do ttl) ou com erros de bit (corrupcao binaria) 
    seja por falta de mecanismos de checksum na camada de enlace ou por falhas na bufferização de routers no core da rede.
*/
int rdt_send_static(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst)
{
    pkt p, ack;
    struct sockaddr_in dst_ack;
    int ns, nr, addrlen;
    struct timeval start, end;
    long elapsed_time;

    /*
        Dado o espaco de numeros sequenciais do remetente e receptor,
        se nao ha nenhum ACK para o pacote send_base partindo do receptor
        nos iremos retransmitir o pacote independente se o mesmo ja chegou
        no receptor.

        Eventos e acoes do SR remetente:
            - DATA ENCAPSULATION: Quando dados sao recebidos da camada de aplicacao (numa implementacao em um protocolo da camada de transporte),
            o remetente checa o proximo numero de sequencia disponivel para o pacote.
            O pacote eh enviado se o numero de sequencia esta dentro da janela de sequencia do remetente, caso nao esteja nela
            os dados sao retornados a camada de aplicacao e retransmitidos mais tarde
            
            - TIMEOUT: Para nos protegermos da perda de pacotes, usaremos temporizacao, na qual os pacotes sao retransmitidos apos 
            estouro do timeout;
            
            - ACK: Se o pacote eh recebido o SR remetente marca aquele pkt como recebido ACK=1. Se o sequence number do pacote
            eh igual ao send_base, a janela de transmissao eh movida para frente
    */

    // Erro caso a janela de transmissao esteja cheia

    if (snd_base + window_size - 1 < snd_base) 
        return ERROR;

    /*
        ------------------------- Primeira transicao da FSM do RDT ARQ (RDT2) -------------------------

        Vamos criar um pacote que encapsula os dados da aplicacao. O proximo numero de sequencia deve ser

    */
    if (make_pkt(&p, PKT_DATA, snd_base + window_size - 1, buf, buf_len) < 0)
        return ERROR;

    // Armazeno o
    snd_buff[(snd_base + window_size - 1) % MAX_WINDOW_SIZE] = p;
    rcv_ack[(snd_base + window_size - 1) % MAX_WINDOW_SIZE] = 0;

    // Sou capaz de injetar erros nas mensagens para alterar o checksum
    if (biterror_inject)
        memset(p.msg, 0, MAX_MSG_LEN);

resend:
    gettimeofday(&start, NULL);

    // Chamo o metodo de envio do protocolo UDP e envio o pacote atraves do socket UDP
    ns = sendto(sockfd, &p, p.h.pkt_size, 0, (struct sockaddr *)dst, sizeof(struct sockaddr_in));

    if (ns < 0)
    {
        perror("rdt_send: sendto(PKT_DATA):");
        return ERROR;
    }

    addrlen = sizeof(struct sockaddr_in);

    /*
        ------------------------- Segunda e Terceira transicao da FSM do RDT ARQ (RDT2) -------------------------

        So vamos esperar retorno do destinatario para confirmar o ACK,
        caso seja negado iremos acionar goto resend para retransmitir o pacote
    */
    nr = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&dst_ack, (socklen_t *)&addrlen);
    gettimeofday(&end, NULL);

    elapsed_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

    if (elapsed_time > MAX_TIMEOUT)
    {
        printf("Timeout Exceeded\n");
        goto resend;
    }

    // Verificamos se o pacote ack esta corrompido e se ele segue a sequenciacao numerica
    if (nr < 0 || iscorrupted(&ack) || !has_ackseq(&ack, snd_base) || !is_the_pkt_duplicated(&ack, snd_base))
    {
        printf("rdt_send: recvfrom(PKT_ACK) || rdt_send: ACK iscorrupted || ACK !has_ackseq\n");
        goto resend;
    }

    // Confirmo o ACK no receptor, uma vez que os dados retornaram integros,
    // e seguindo a sequenciacao numerica correta
    rcv_ack[ack.h.pkt_seq % MAX_WINDOW_SIZE] = PKT_ACK;

    // Apenas prossigo a janela de transmissao
    while(rcv_ack[snd_base % MAX_WINDOW_SIZE])
        snd_base++;

    return buf_len;
}

/*
    O protocolo RDT implementado na camada de aplicação possui uma função que é chamada
    pelo protocolo UDP quando um segmento chega no lado receptor do canal não confiável
    (que está em parte na camada de rede).
*/
int rdt_recv_static(int sockfd, void *buf, int buf_len, struct sockaddr_in *src)
{
    pkt p, ack;
    int nr, ns;
    int addrlen;

     /*
        Eventos e acoes de um SR receptor:
        - Sequence Number PKT em  
    */

    // Preparo um pacote buffer para receber dados do remetente
    memset(&p, 0, sizeof(hdr));

    // Crio um pacote de ACK
    if (make_pkt(&ack, PKT_ACK, rcv_base - 1, NULL, 0) < 0)
        return ERROR;

rerecv:
    addrlen = sizeof(struct sockaddr_in);

    nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr *)src, (socklen_t *)&addrlen);

    if (nr < 0)
    {
        perror("recvfrom():");
        return ERROR;
    }

    // se o pacote esta corrompido ou chegou fora de ordem (perdemos um pacote no caminho)
    if (iscorrupted(&p) || !has_dataseqnum(&p, rcv_base))
    {
        printf("rdt_recv: iscorrupted || has_dataseqnum\n");

        // Vou solicitar ao cliente que reenvie um pacote
        ns = sendto(sockfd, &ack, ack.h.pkt_size, 0, (struct sockaddr *)src, (socklen_t)sizeof(struct sockaddr_in));

        if (ns < 0)
        {
            perror("rdt_rcv: sendto(PKT_ACK - 1)");
            return ERROR;
        }

        goto rerecv;
    }

    int msg_size = p.h.pkt_size - sizeof(hdr);

    if (msg_size > buf_len)
    {
        printf("rdt_rcv(): tamanho insuficiente de buf (%d) para payload (%d).\n", buf_len, msg_size);
        // O QUE EU FACO NUM ERRO DE PAYLOAD??????????????W
        return ERROR;
    }

    // Copio a mensagem recebida para um buffer a fim de processa-la
    memcpy(buf, p.msg, msg_size);

    if (make_pkt(&ack, PKT_ACK, p.h.pkt_seq, NULL, 0) < 0)
        return ERROR;

    // Confirmo que recebi o pacote
    ns = sendto(sockfd, &ack, ack.h.pkt_size, 0, (struct sockaddr *)src, (socklen_t)sizeof(struct sockaddr_in));
    if (ns < 0)
    {
        perror("rdt_rcv: sendto(PKT_ACK)");
        return ERROR;
    }

    rcv_base++;

    return p.h.pkt_size - sizeof(hdr);
}

/*
    Retransmissão no Sender (rdt_send_dynamic):
    - Adicionado loop para até max_retries retransmissões.
    - Usa select() para esperar por ACKs com timeout ajustável.
    - Atualiza dinamicamente o tamanho da janela (AIMD) e o timeout após cada retransmissão.
    - Trata ACKs inválidos/corrompidos como falha, reiniciando o processo.
*/
int rdt_send_dynamic(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst) {
    pkt p, ack;
    struct sockaddr_in dst_ack;
    int ns, nr, addrlen;
    struct timeval start, end, tv;
    fd_set readfds;
    int retries = 0;
    const int max_retries = 5;  // Máximo de retransmissões
    long elapsed_time;

    if (snd_base + window_size - 1 < snd_base)
        return ERROR;

    if (make_pkt(&p, PKT_DATA, snd_base + window_size - 1, buf, buf_len) < 0)
        return ERROR;

    snd_buff[(snd_base + window_size - 1) % MAX_WINDOW_SIZE] = p;
    rcv_ack[(snd_base + window_size - 1) % MAX_WINDOW_SIZE] = 0;

    if (biterror_inject) {
        memset(p.msg, 0, MAX_MSG_LEN);
    }

    // Loop de retransmissão
    while (retries < max_retries) {
        gettimeofday(&start, NULL);
        ns = sendto(sockfd, &p, p.h.pkt_size, 0, (struct sockaddr *)dst, sizeof(struct sockaddr_in));

        if (ns < 0) {
            perror("rdt_send: sendto(PKT_DATA):");
            return ERROR;
        }

        // Configuro timeout com select()
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;

        int sel = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        gettimeofday(&end, NULL);

        // Calculo tempo decorrido
        elapsed_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

        if (sel == 0) {
            // Timeout: retransmitir e ajustar janela
            printf("Timeout: retransmitindo (tentativa %d)\n", retries + 1);
            retries++;
            window_size = (int)(window_size * AIMD_DECREASE_FACTOR);  // AIMD
            timeout = (timeout + elapsed_time) / 2;  // Ajuste dinâmico
            if (timeout > MAX_TIMEOUT) timeout = MAX_TIMEOUT;
            continue;
        } else if (sel < 0) {
            perror("select()");
            return ERROR;
        }

        // Recebo ACK
        addrlen = sizeof(struct sockaddr_in);
        nr = recvfrom(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&dst_ack, (socklen_t *)&addrlen);

        if (nr < 0) {
            perror("recvfrom()");
            return ERROR;
        }

        // Verifico ACK
        if (iscorrupted(&ack) || !has_ackseq(&ack, snd_base) || is_the_pkt_duplicated(&ack, snd_base)) {
            printf("ACK inválido/corrompido (retransmitindo)\n");
            retries++;
            timeout = (timeout + elapsed_time) / 2;  // Atualizo timeout
            continue;
        }

        // ACK válido: atualizo base e janela
        rcv_ack[ack.h.pkt_seq % MAX_WINDOW_SIZE] = PKT_ACK;

        // Avanço a janela de transmissão
        while (rcv_ack[snd_base % MAX_WINDOW_SIZE]) {
            snd_base++;
        }

        // Aumento janela (AIMD)
        if (window_size < MAX_WINDOW_SIZE) {
            window_size += AIMD_INCREMENT;
        } else {
            window_size = (int)(window_size * AIMD_DECREASE_FACTOR);
        }

        // Atualizo timeout com o RTT medido
        timeout = (timeout + elapsed_time) / 2;
        break;
    }

    if (retries >= max_retries) {
        printf("ERRO: Máximo de retransmissões atingido.\n");
        return ERROR;
    }

    return buf_len;
}

/*
    Envia ACKs repetidos para pacotes fora de ordem ou corrompidos.
    Mantém a lógica de sequência para garantir ordenação.
*/

int rdt_recv_dynamic(int sockfd, void *buf, int buf_len, struct sockaddr_in *src) {
    pkt p, ack;
    int nr, ns;
    int addrlen;

    memset(&p, 0, sizeof(hdr));

    if (make_pkt(&ack, PKT_ACK, rcv_base - 1, NULL, 0) < 0)
        return ERROR;

rerecv:
    addrlen = sizeof(struct sockaddr_in);
    nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr *)src, (socklen_t *)&addrlen);

    if (nr < 0) {
        perror("recvfrom()");
        return ERROR;
    }

    // Verificar se o pacote está na sequência esperada
    if (iscorrupted(&p) || !has_dataseqnum(&p, rcv_base)) {
        printf("Pacote corrompido/fora de ordem: enviando ACK anterior\n");

        // Enviar ACK do último pacote válido
        ns = sendto(sockfd, &ack, ack.h.pkt_size, 0, (struct sockaddr *)src, (socklen_t)sizeof(struct sockaddr_in));

        if (ns < 0) {
            perror("sendto()");
            return ERROR;
        }

        goto rerecv;  // Esperar pelo pacote correto
    }

    // Copiar dados para o buffer
    int msg_size = p.h.pkt_size - sizeof(hdr);
    if (msg_size > buf_len) {
        printf("Buffer insuficiente (necessário: %d)\n", msg_size);
        return ERROR;
    }

    memcpy(buf, p.msg, msg_size);

    // Enviar ACK para o pacote recebido
    if (make_pkt(&ack, PKT_ACK, p.h.pkt_seq, NULL, 0) < 0)
        return ERROR;

    ns = sendto(sockfd, &ack, ack.h.pkt_size, 0, (struct sockaddr *)src, (socklen_t)sizeof(struct sockaddr_in));
    if (ns < 0) {
        perror("sendto()");
        return ERROR;
    }

    rcv_base++;  // Avançar a base do receptor
    return msg_size;
}