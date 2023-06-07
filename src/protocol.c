#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"
#include "rawsocket.h"
#include "utils.h"

unsigned int seq = 0;
unsigned int last_seq = MAXSEQ;

size_t tmp = 0;

msg_t *send_and_wait(int socket, int type, void *data, unsigned int size, int wait_type)
{
    msg_t *response = NULL;
    msg_t *sent = send_message(socket, type, data, size);
    while (!response) {
        response = wait_valid_message(socket);
        if (!response)
            return NULL;
        if (response->type == ERR) {
            // TODO: extract actual error code
            errno = ERR;
            return destroy_message(response);
        } else if (response->type == NACK) {
            DECSEQ(last_seq);
            response = destroy_message(response);
            // sleep(2);
            sent = resend_message(socket, sent);
        } else if (wait_type != -1 && response->type != wait_type) {
            response = destroy_message(response);
            sent = resend_message(socket, sent);
        }
    }
    destroy_message(sent);
    return response;
}

msg_t *resend_and_wait(int socket, int type, void *data, unsigned int size, int wait_type)
{
    DECSEQ(seq);
    return send_and_wait(socket, type, data, size, wait_type);
}

int receive_file(int socket, int send_ok, char *filename)
{
    unsigned char msgbuf[MAX_DATA_BYTES];
    unsigned int bytecount;
    msg_t *response = NULL, *sent = NULL;
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        msgbuf[0] = errno_to_code(errno);
        sent = send_message(socket, ERR, msgbuf, 1);
        // idepende se deu problema ou nao, soh finaliza
        printf("WAITING FOR INPUT :");
        getchar();
        destroy_message(sent);
        return errno;
    }
    // caso precise mandar ok isso eh recebido como parametro
    if (send_ok) {
        response = send_and_wait(socket, OK, NULL, 0, DATA);
        if (!response) {
            fclose(fp);
            return errno;
        }
    } else {
        response = send_and_wait(socket, RECV_FILE, filename, strlen(filename), -1);
        if (!response) {
            fclose(fp);
            return errno;
        }
    }
    // caso contrario response ja vem com DATA
    while (response->type != END_FILE) {
        if (response->type == DATA) {
            // bytecount = fread(msgbuf, sizeof *msgbuf, MAX_DATA_BYTES, fp);
            bytecount = fwrite(response->data, sizeof(unsigned char), response->size, fp);
            if (bytecount != response->size) {
                // TODO: err
                fprintf(stderr, "RECEIVED %d BYTES. %d BYTES WRITTEN!\n", response->size, bytecount);
            }
            response = destroy_message(response);
            response = send_and_wait(socket, ACK, NULL, 0, -1);
            if (!response) {
                fclose(fp);
                return errno;
            }
            while (response->type != END_FILE && response->type != DATA) {
                response = resend_and_wait(socket, NACK, NULL, 0, -1);
                if (!response) {
                    fclose(fp);
                    return errno;
                }
            }
        } else {
            while (response->type != END_FILE && response->type != DATA) {
                response = resend_and_wait(socket, NACK, NULL, 0, -1);
                if (!response) {
                    fclose(fp);
                    return errno;
                }
            }
        }
    }
    fclose(fp);
    return 0;
}

int receive_batch(int socket, int send_ok, char *regex)
{
    int res;
    msg_t *response = NULL;
    // caso precise mandar ok isso eh recebido como parametro
    if (send_ok) {
        response = send_and_wait(socket, OK, NULL, 0, -1);
        if (!response)
            return errno;
    } else {
        char buf[1024];
        memset(buf, 0, 1024 * sizeof *buf);
        if (regex[0] == '*') {
            buf[0] = '.';
            strncpy(buf + 1, regex, strlen(regex));
        } else {
            strncpy(buf, regex, strlen(regex));
        }
        response = send_and_wait(socket, RECV_BATCH, buf, strlen(buf), -1);
        if (!response)
            return errno;
    }
    // caso contrario response ja vem com DATA
    while (response->type != END_BATCH) {
        if (response->type == RECV_NAME || response->type == BACKUP_FILE) {
            if ((res = receive_file(socket, 1, (char *)response->data)))
                return res;
            destroy_message(response);
            response = send_and_wait(socket, OK, NULL, 0, -1);
            if (!response)
                return -1;
        } else {
            destroy_message(response);
            DECSEQ(seq);
            response = send_and_wait(socket, NACK, NULL, 0, -1);
            if (!response)
                return -1;
        }
    }
    return 0;
}

int send_file(int socket, int init_type, char *filename)
{
    unsigned char msgbuf[MAX_DATA_BYTES];
    unsigned int bytecount;
    msg_t *response = NULL, *sent;
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        if (init_type != BACKUP_FILE) {
            msgbuf[0] = errno_to_code(errno);
            sent = send_message(socket, ERR, msgbuf, 1);
            // idepende se deu problema ou nao, soh finaliza
            destroy_message(sent);
        }
        return errno;
    }
    if (init_type >= 0) {
        response = send_and_wait(socket, init_type, filename, strlen(filename), OK);
        if (!response) {
            fclose(fp);
            return errno;
        }
    }
    destroy_message(response);
    while (!feof(fp)) {
        memset(msgbuf, 0, MAX_DATA_BYTES * sizeof *msgbuf);
        bytecount = fread(msgbuf, sizeof *msgbuf, MAX_DATA_BYTES, fp);
        response = send_and_wait(socket, DATA, msgbuf, bytecount, ACK);
        if (!response) {
            fclose(fp);
            return errno;
        }
        destroy_message(response);
    }
    sent = send_message(socket, END_FILE, NULL, 0);
    destroy_message(sent);
    fclose(fp);
    return 0;
}

int send_batch(int socket, int t, int init_type, char **filenames, unsigned int file_count)
{
    int res;
    msg_t *response = NULL, *sent;
    if (t >= 0) {
        response = send_and_wait(socket, t, &file_count, sizeof(file_count), OK);
        if (!response)
            return errno;
        destroy_message(response);
    }
    for (unsigned int i = 0; i < file_count; ++i)
        if ((res = send_file(socket, init_type, filenames[i])))
            return res;
    sent = send_message(socket, END_BATCH, &file_count, sizeof(file_count));
    destroy_message(sent);
    return 0;
}

void send_error(int socket, int error_code)
{
    msg_t *tmp = send_message(socket, ERR, &error_code, sizeof(error_code));
    destroy_message(tmp);
}
