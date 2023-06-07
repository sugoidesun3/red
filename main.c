#include <stdio.h>
#include "shell.h"
#include "server.h"
#include "rawsocket.h"
#include "protocol.h"
#include <malloc.h>

#define DEVICE "enp3s0"


int main(void) {
    int socket = open_socket(DEVICE);
    printf("OPENED SOCKET: %d\n", socket);

    shell_rpel(socket);
    // server_loop(socket);

    return 0;
}
