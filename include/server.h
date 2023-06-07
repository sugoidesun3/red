#ifndef __REDES_T1_SERVER_H
#define __REDES_T1_SERVER_H

#include "protocol.h"

void server_loop(int socket);

int request_handler(int socket, msg_t *received);

#endif /* __REDES_T1_SERVER_H */
