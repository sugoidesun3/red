#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "rawsocket.h"

extern unsigned int seq;
extern unsigned int last_seq;

#define TIMEOUT_SECONDS 3

int open_socket(char *device)
{
    int soquete;
    struct ifreq ir;
    struct sockaddr_ll endereco;
    struct packet_mreq mr;
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SECONDS;
    tv.tv_usec = 0;

    soquete = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (soquete == -1) {
        printf("Erro no Socket\n");
        exit(-1);
    }

    memset(&ir, 0, sizeof ir);
    memcpy(ir.ifr_name, device, strlen(device));
    if (ioctl(soquete, SIOCGIFINDEX, &ir) == -1) {
        printf("Erro no ioctl\n");
        exit(-1);
    }

    memset(&endereco, 0, sizeof(endereco));
    endereco.sll_family = AF_PACKET;
    endereco.sll_protocol = htons(ETH_P_ALL);
    endereco.sll_ifindex = ir.ifr_ifindex;
    if (bind(soquete, (struct sockaddr *)&endereco, sizeof(endereco)) == -1) {
        printf("Erro no bind\n");
        exit(-1);
    }

    memset(&mr, 0, sizeof(mr));
    mr.mr_ifindex = ir.ifr_ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    //if (setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
    if (setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
        printf("Erro ao fazer setsockopt\n");
        exit(-1);
    }
    if (setsockopt(soquete, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
        printf("Erro ao fazer setsockopt 2\n");
        exit(-1);
    }

    return soquete;
}

msg_t *create_message(unsigned int type, void *data, unsigned int size)
{
    unsigned int i;
    msg_t *message = malloc(sizeof *message);
    if (!message)
        return NULL;
    message->data = malloc(size);
    if (!message->data)
        return destroy_message(message);
    message->init_mark = INIT_MARK;
    message->size = size;
    message->seq = seq;
    message->type = type;
    // paridade par :)
    message->parity = 0;
    // memcpy(message->data, data, size);
    for (i = 0; i < size; ++i) {
        ((unsigned char *)message->data)[i] = ((unsigned char *)data)[i];
        message->parity ^= ((unsigned char *)message->data)[i];
    }
    // incrementar sequencia global
    INCSEQ(seq);
    return message;
}

unsigned char *pack_message(msg_t *message, unsigned int *bytecount)
{
    // TODO: simplificar criando um macro "TRUESIZE(...)"
    // -8 (8 bytes do ponteiro) + message->size (# bytes dos dados)
    unsigned int truesize = 0;
    unsigned char *msgbytes;
    if (message->size < INTERFACE_MIN_DATA_BYTES)
        *bytecount = INTERFACE_MIN_DATA_BYTES + MIN_MESSAGE_BYTES;
    else
        *bytecount = message->size + MIN_MESSAGE_BYTES;
    truesize = *bytecount - MIN_MESSAGE_BYTES;

#ifdef NEEDS_ETHERNET_HEADER
    *bytecount += sizeof(eth_t);
#endif /* NEEDS_ETHERNET_HEADER */

    msgbytes = malloc(*bytecount);
    if (!msgbytes)
        return NULL;
    memset(msgbytes, 0, *bytecount);

    unsigned char parity = message->parity;

#ifdef NEEDS_ETHERNET_HEADER
    eth_t *aux1 = (eth_t *)msgbytes;
    aux1->b[0] = 0x81;
    aux1->b[1] = 0x00;
    msg_t *aux = (msg_t *)((unsigned char *)msgbytes + sizeof(eth_t));
#else
    msg_t *aux = (msg_t *)msgbytes;
#endif /* NEEDS_ETHERNET_HEADER */

    aux->init_mark = message->init_mark;
    aux->size = message->size;
    aux->seq = message->seq;
    aux->type = message->type;
    // memcpy(msgbytes, message, 3);
    memcpy((unsigned char *)aux + 3, message->data, message->size);

    memcpy((unsigned char *)aux + 3 + truesize, &parity, 1);
    return msgbytes;
}

msg_t *unpack_message(unsigned char *msgbytes)
{
    unsigned int p;
    // TODO: fazer cast (BEM + rapido que malloc, mas + complicado)
    msg_t *msg = malloc(sizeof *msg);
    if (!msg)
        return NULL;
    memset(msg, 0, sizeof *msg);
#ifdef  NEEDS_ETHERNET_HEADER
    msg_t *aux = (msg_t *)(msgbytes + sizeof(eth_t) - 4);
#else
    msg_t *aux = (msg_t *)msgbytes;
#endif /* NEEDS_ETHERNET_HEADER */
    msg->init_mark = aux->init_mark;
    msg->size = aux->size;
    msg->seq = aux->seq;
    msg->type = aux->type;
    msg->data = malloc(msg->size);
    if (!msg->data)
        return destroy_message(msg);
    memcpy(msg->data, (unsigned char *)aux + 3, msg->size);

    if (msg->size < INTERFACE_MIN_DATA_BYTES)
        memcpy(&p, (unsigned char *)aux + 3 + INTERFACE_MIN_DATA_BYTES, 1);
    else
        memcpy(&p, (unsigned char *)aux + 3 + msg->size, 1);
    msg->parity = p;
    return msg;
}

int validate_parity(msg_t *msg)
{
    unsigned int parity = 0;
    for (unsigned int i = 0; i < msg->size; ++i)
        msg->parity ^= ((unsigned char *)msg->data)[i];
    return parity == msg->parity;
}

msg_t *destroy_message(msg_t *msg)
{
    if (!msg)
        return NULL;
    free(msg->data);
    free(msg);
    return NULL;
}

void print_message(msg_t *msg)
{
    if (!msg)
        return;
    printf("|%02x|", msg->init_mark);
    printf("%02x|", msg->size);
    printf("%02x|", msg->seq);
    printf("(%x)|", msg->type);
    if (msg->data && msg->size) {
        for (int i = 0; i < msg->size; ++i) {
            printf("%02x", ((unsigned char *)msg->data)[i]);
        }
        printf("|");
    } else
        printf("-|");
    printf("%02x|", msg->parity);
}

msg_t *resend_message(int socket, msg_t *msg)
{
    unsigned int bytecount;
    unsigned char *msgbuf = pack_message(msg, &bytecount);
    if (!msgbuf) {
        destroy_message(msg);
        return NULL;
    }
    send(socket, msgbuf, bytecount, 0);
    free(msgbuf);
    return msg;
}

msg_t *send_message(int socket, int type, void *data, unsigned int size)
{
    // unsigned char msgbuf[MAX_MESSAGE_BYTES];
    unsigned char *msgbuf;
    unsigned int bytecount, sent;
    msg_t *msg = create_message(type, data, size);
    if (!msg) {
        fprintf(stderr, "1???\n");
        return NULL;
    }
    msgbuf = pack_message(msg, &bytecount);
    if (!msgbuf) {
        fprintf(stderr, "2???\n");
        destroy_message(msg);
        return NULL;
    }
#ifdef DEBUG
    printf(">>> SEND (SEQ %d): ", msg->seq);
    print_message(msg);
    printf(" === ");
    for (unsigned int i = 0; i < bytecount; ++i)
        printf("%02x", msgbuf[i]);
    printf("\n");
#endif /* DEBUG */
    if ((sent = send(socket, msgbuf, bytecount, 0)) != bytecount)
        fprintf(stderr, "%d of %d SENT?\n", sent, bytecount);
    // free(msgbuf);
    return msg;
}

msg_t *wait_message(int socket)
{
    int c;
#ifdef DEBUG
    printf("WAITING...\n");
#endif /* DEBUG */
    msg_t *unpacked;
    unsigned char buf[RECV_SIZE];
    do {
        memset(buf, 0, RECV_SIZE * sizeof *buf);
        if ((c = recv(socket, buf, RECV_SIZE, 0)) < 0) {
            errno = -1;
            return NULL;
        }
        // possivel problema: marcador de inicio corrompido (fodeu)
#ifdef  NEEDS_ETHERNET_HEADER
    } while(buf[12] != INIT_MARK);
#else
    } while(buf[0] != INIT_MARK);
#endif /* NEEDS_ETHERNET_HEADER */
    unpacked = unpack_message(buf);

#ifdef DEBUG
    printf(">>> RECV (SEQ %d): ", unpacked->seq);
    print_message(unpacked);
    printf(" === ");
    for (int i = 0; i < c; ++i)
        printf("%02x", buf[i]);
    printf("\n");
#endif /* ifdef DEBUG */

    return unpacked;
}

msg_t *wait_valid_message(int socket)
{
    msg_t *msg = NULL;
    while (!msg) {
        msg = wait_message(socket);
        if (!msg)
            return NULL;
        // tipo errado (nao definido) = deu caca
        if (msg->type == 0xb) {
#ifdef DEBUG
            printf("> [ERR] INVALID MESSAGE! INVALID TYPE!\n");
#endif /* ifdef DEBUG */
            msg = destroy_message(msg);
        // paridade errada = deu caca
        } else if (!validate_parity(msg)) {
#ifdef DEBUG
            printf("> [ERR] INVALID MESSAGE! WRONG PARITY!\n");
#endif /* ifdef DEBUG */
            msg = destroy_message(msg);
        // TODO: sequencia errada = deu caca
        } else if (NEXTSEQ(last_seq) != msg->seq) {
#ifdef DEBUG
            printf("> [ERR] INVALID MESSAGE! WRONG SEQUENCE! (%d instead of %d)\n", msg->seq, NEXTSEQ(last_seq));
            //getchar();
#endif /* ifdef DEBUG */
            msg = destroy_message(msg);
        }
        if (!msg) {
            DECSEQ(seq);
            send_message(socket, NACK, NULL, 0);
        } else {
            last_seq = msg->seq;
        }
    }
    return msg;
}
