#ifndef RDT_H
#define RDT_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>

#define MAX_MSG_LEN 1000
#define TRUE 1
#define FALSE 0
#define ERROR -1
#define SUCCESS 0

// Tempo inicial e maximo de timeout em milissegundos
#define INITIAL_TIMEOUT 1000 
#define MAX_TIMEOUT 7000     

// Para a janela de transmissao estatica usamos um WINDOW_SIZE de tamanho N
// para pacotes pendentes, e não reconhecidos no pipeline (vetor)
#define WINDOW_SIZE 5

// Para a janela de transmissao dinamica, usando AIMD
// usamos um WINDOW_SIZE de tamanho N para pacotes 
// pendentes, e não reconhecidos no pipeline (vetor)
#define MAX_WINDOW_SIZE 10
#define AIMD_INCREMENT 1
#define AIMD_DECREASE_FACTOR 0.5

// unsigned 16-bit integer eh um tipo de binario de 16 bits
// pertencente aos campos do header de um segmento
typedef uint16_t hsize_t;
typedef uint16_t hcsum_t;
typedef uint16_t hseq_t;
typedef uint8_t  htype_t;

#define PKT_NACK 0
#define PKT_ACK 1
#define PKT_DATA 2

// Essa eh a estrutura do cabeçalho do pacote
struct hdr{
    hsize_t pkt_size;  // Tamanho do pacote
    hcsum_t csum;      // Checksum
    hseq_t pkt_type;   // Tipo do pacote (DATA, ACK)
    htype_t pkt_seq;    // Número de sequência
};

typedef struct hdr hdr;

// Estrutura do pacote
struct pkt {
    hdr h;              // Cabeçalho
    char msg[MAX_MSG_LEN]; // Mensagem
};

typedef struct pkt pkt;

unsigned short checksum(unsigned short *, int);
int iscorrupted(pkt *);
int make_pkt(pkt *, htype_t, hseq_t, void *, int);
int has_ackseq(pkt *, hseq_t);
int rdt_send(int, void *, int, struct sockaddr_in *);
int has_dataseqnum(pkt *, hseq_t);
int rdt_recv(int, void *, int, struct sockaddr_in *);

#endif