#ifndef __REDES_T1_PROTOCOL_H
#define __REDES_T1_PROTOCOL_H

// 0x7E == 0b01111110
#define INIT_MARK 0x7E

#define MIN_MESSAGE_BYTES 4
#define MAX_DATA_BYTES 63
#define MAX_MESSAGE_BYTES ((MIN_MESSAGE_BYTES) + (MAX_DATA_BYTES))

#define SEQBITS 4
#define MAXSEQ ((1 << (SEQBITS)) - 1)

#define PREVSEQ(seq) (((int)(seq) - 1) < 0 ? (MAXSEQ) : ((seq) - 1))
#define NEXTSEQ(seq) (((seq) + 1) % ((MAXSEQ) + 1))
#define INCSEQ(seq) ((seq) = (NEXTSEQ(seq)))
#define DECSEQ(seq) ((seq) = (PREVSEQ(seq)))

#define NEEDS_ETHERNET_HEADER 1

typedef struct __attribute__((__packed__)){
    unsigned char dst[6];
    unsigned char src[6];
    unsigned char b[4];
} eth_t;

#ifdef NEEDS_ETHERNET_HEADER
#define RECV_SIZE ((MAX_MESSAGE_BYTES) + sizeof(eth_t) - 4)
#else
#define RECV_SIZE (MAX_MESSAGE_BYTES)
#endif /* NEEDS_ETHERNET_HEADER */

// message types
#define BACKUP_FILE  0x0
#define BACKUP_BATCH 0x1
#define RECV_FILE    0x2
#define RECV_BATCH   0x3
#define CHDIR        0x4
#define VERIFY       0x5
#define RECV_NAME    0x6
#define RECV_MD5     0x7
#define DATA         0x8
#define END_FILE     0x9
#define END_BATCH    0xa
// ...               0xb
#define ERR          0xc
#define OK           0xd
#define ACK          0xe
#define NACK         0xf

#define FULL_DISK           0x0
#define NO_WRITE_PERMISSION 0x1
#define FILE_NOT_FOUND      0x2
#define NO_READ_PERMISSION  0x3

typedef struct __attribute__((packed)) {
    unsigned int init_mark : 8;
    unsigned int size : 6;
    unsigned int seq: SEQBITS;
    unsigned int type : 6;
    void *data;
    unsigned int parity : 8;
} msg_t;

// int request_handler(int socket, msg_t **last_msg, char **ctx_fn, FILE **ctx_fp, int last_type);

msg_t *send_and_wait(int socket, int type, void *data, unsigned int size, int wait_type);

msg_t *resend_and_wait(int socket, int type, void *data, unsigned int size, int wait_type);

void send_error(int socket, int error_code);

int send_file(int socket, int init_type, char *filename);

int receive_file(int socket, int send_ok, char *filename);

int receive_batch(int socket, int send_ok, char *regex);

int send_batch(int socket, int t, int init_type, char **filenames, unsigned int file_count);

#endif /* __REDES_T1_PROTOCOL_H */
