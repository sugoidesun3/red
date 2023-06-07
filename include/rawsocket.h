#ifndef __REDES_T1_RAWSOCKET_H
#define __REDES_T1_RAWSOCKET_H

#include "protocol.h"

#define INTERFACE_MIN_BYTES 14
#define INTERFACE_MIN_DATA_BYTES ((INTERFACE_MIN_BYTES) - (MIN_MESSAGE_BYTES))

extern unsigned int seq;


int open_socket(char *device);


msg_t *create_message(unsigned int type, void *data, unsigned int size);

msg_t *destroy_message(msg_t *msg);


msg_t *send_message(int socket, int type, void *data, unsigned int size);

msg_t *resend_message(int socket, msg_t *msg);


// TODO: timeout
msg_t *wait_message(int socket);

msg_t *wait_valid_message(int socket);


unsigned char *pack_message(msg_t *msg, unsigned int *bytecount);

msg_t *unpack_message(unsigned char *msgbytes);

int validate_parity(msg_t *msg);


void print_message(msg_t *msg);

#endif /* __REDES_T1_RAWSOCKET_H */
