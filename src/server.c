#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <errno.h>
#include "server.h"
#include "rawsocket.h"
#include "protocol.h"
#include "utils.h"

#define BUFSIZE 64

extern unsigned int seq;
extern unsigned int last_seq;
extern int syncseqfirst;

void server_loop(int socket)
{
    int done = 0, err = 0;
    msg_t *received, *last_sent = NULL;

    // char cwd[PATH_MAX];
    // char ocwd[PATH_MAX];
    // system("mkdir -p ./data");
    if (chdir("data")) {
        fprintf(stderr, "NAO FOI POSSIVEL ENTRAR NO DIRETORIO DO PROJETO!\n");
        fprintf(stderr, "\t> (%d) %s!\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    //if (!getcwd(ocwd, PATH_MAX)) {
        //fprintf(stderr, "NO PWD? %s\n", cwd);
        //exit(EXIT_FAILURE);
    //}
    while (!done) {
        received = wait_valid_message(socket);
        // deu timeout? espera de novo kkkkkkkkk
        if (!received)
            continue;
        if ((err = request_handler(socket, received))) {
            switch (err) {
                case NACK:
                    fprintf(stderr, "[ERROR] COMANDO INVALIDO!\n");
                    received = destroy_message(received);
                    destroy_message(last_sent);
                    DECSEQ(seq);
                    last_sent = send_message(socket, NACK, NULL, 0);
                    break;
                case -1:
                    // destroy_message(send_message(socket, FIXSEQ, NULL, 0));
                    syncseqfirst = 1;
                    fprintf(stderr, "[ERROR] TIMEOUT DE 3 SEGUNDOS EXCEDIDO!\n");
                    break;
                default:
                    fprintf(stderr, "[ERROR] ERRO DESCONHECIDO!\n");
                    break;
            }
        }
    }
}

int request_handler(int socket, msg_t *received)
{
    unsigned int count;
    msg_t *message;
    char **fs;
    unsigned char databuf[MAX_DATA_BYTES + 1];
    unsigned char *data_send;
    memcpy(databuf, received->data, received->size);
    // padding zero
    databuf[received->size] = 0;
    // apenas espera-se um comando, qualquer outra coisa eh um NACK
    switch (received->type) {
        // TODO: qualquer erro assume-se erro de timeout, arrumar isso
        case BACKUP_FILE:
            if (receive_file(socket, 1, (char *)databuf)) {
                // printf("ERRO NO RECEIVE_FILE!! (%s)\n", strerror(errno));
                return -1;
            }
            break;
        case BACKUP_BATCH:
            if (receive_batch(socket, 1, NULL)) {
                // printf("ERRO NO RECEIVE_BATCH!! (%s)\n", strerror(errno));
                return -1;
            }
            break;
        case RECV_FILE:
            if (send_file(socket, -1, (char *)databuf)) {
                // printf("ERRO NO SEND_FILE!! (%s)\n", strerror(errno));
                return -1;
            }
            break;
        case RECV_BATCH:
            fs = get_filenames((char *) databuf, &count);
            if (send_batch(socket, -1, RECV_NAME, fs, count)) {
                // printf("ERRO NO SEND_BATCH!! (%s)\n", strerror(errno));
                return -1;
            }
            for (unsigned int i = 0; i < count; ++i)
                free(fs[i]);
            free(fs);
            break;
        case CHDIR:
            chdir((char *)databuf);
            message = send_message(socket, OK, NULL, 0);
            break;
        case VERIFY:
            if (!file_exists((char *)databuf)) {
                send_error(socket, FILE_NOT_FOUND);
                return 0;
            }
            data_send = calc_md5sum((char *)databuf);
            message = send_message(socket, RECV_MD5, data_send, MD5_DIGEST_LENGTH);
            break;
        default:
            DECSEQ(seq);
            message = send_message(socket, NACK, NULL, 0);
            destroy_message(message);
            return NACK; // ????????
    }
    destroy_message(message);
    return 0;
}
