#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include "protocol.h"
#include "rawsocket.h"
#include "shell.h"
#include "utils.h"
#include <openssl/md5.h>

#define BUFSIZE 64

void shell_rpel(int socket)
{
    char *line;
    char **args;
    int status = 0;
    char cwd[PATH_MAX];
    char ocwd[PATH_MAX];
    system("mkdir -p ./data");
    if (chdir("data")) {
        fprintf(stderr, "NAO FOI POSSIVEL ENTRAR NO DIRETORIO DO PROJETO!\n");
        fprintf(stderr, "\t> (%d) %s!\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (!getcwd(ocwd, PATH_MAX)) {
        fprintf(stderr, "NO PWD? %s\n", cwd);
        exit(EXIT_FAILURE);
    }
    for (;;) {
        print_ps1(cwd);
        line = shell_read_line();
        if (!line) {
            printf(">>> LINE?\n");
            continue;
        }
        args = shell_split_line(line);
        if (!args) {
            printf(">>> ARGS?\n");
            continue;
        }
        status = exec_command(socket, args, ocwd, cwd);
        // TODO: entender e usar errno
        if (status) {
            if (status == -5)
                fprintf(stderr, "ERRO! Nao foi possivel mudar de diretorio!\n");
            else if (errno == -1)
                fprintf(stderr, "ERRO! Timeout de 3 segundos excedido!\n");
            else
                fprintf(stderr, ">>> err? \"%s\"\n", strerror(errno));
            //print_error(...);
        }
        free(line);
        free(args[0]);
        free(args[1]);
    }
}

/* --- shell interface --- */

void print_ps1(char *cwd)
{
    if (!getcwd(cwd, PATH_MAX)) {
        printf("NO PWD? %s\n", cwd);
        exit(EXIT_FAILURE);
    }
    printf("(%s)%s", cwd, PS1);
}

// void print_error(char *description);

/* --- shell utils --- */
char *shell_read_line(void)
{
    char *line = NULL;
    size_t bufsize = 0;
    // have getline allocate a buffer for us
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            printf("?????????????????????\n");
            free(line);
            exit(EXIT_SUCCESS);
        } else  {
            // TODO: usar errno
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char **shell_split_line(char *line)
{
    char **tokens;
    int index = -1;
    int length = (int) strlen(line);
    while (line[++index] != ' ' && index != length - 1);
    tokens = malloc(sizeof *tokens);
    if (!tokens)
        return NULL;
    tokens[0] = malloc(index * sizeof *tokens[0]);
    memset(tokens[0], 0, index * sizeof *tokens[0]);
    if (index == length - 1) {
        strncat(tokens[0], line, index);
        return tokens;
    }
    // TODO: (se sobrar tempo) suporte a mais de um argumento
    tokens[1] = malloc((length - index - 2) * sizeof *tokens[1]);
    memset(tokens[1], 0, (length - index - 2) * sizeof *tokens[1]);
    strncat(tokens[0], line, index);
    strncat(tokens[1], line + index + 1, length - index - 2);
    return tokens;
}


/* --- shell commands --- */

int exec_command(int socket, char **args, char *ocwd, char *cwd)
{
    // printf("EXEC COMMAND '%s' \"%s\"\n", args[0], args[1]);
    char *cmd = args[0];
    char buf[1024];
    memset(buf, 0, 1024 * sizeof *buf);
    if (!strcmp(cmd, "clear"))
        system("clear");
    else if (!strcmp(cmd, "ls"))
        return list_subdirectories(args[1]);
    else if (!strcmp(cmd, "cd"))
        return change_directory(ocwd, cwd, args[1]);
    else if (!strcmp(cmd, "bck"))
        return send_file(socket, BACKUP_FILE, args[1]);
    else if (!strcmp(cmd, "bbck")) {
        if (!args[1]) {
            fprintf(stderr, "ARGUMENT REQUIRED FOR COMMAND \"%s\"!\n", args[0]);
            return -1;
        }
        return batch_backup(socket, args[1]);
    } else if (!strcmp(cmd, "md5")) {
        if (!args[1]) {
            fprintf(stderr, "NO FILE SPECIFIED!\n");
            return -1;
        }
        return check_md5(socket, args[1]);
    } else if (!strcmp(cmd, "recv")) {
        return receive_file(socket, 0, args[1]);
    } else if (!strcmp(cmd, "recvb")) {
        return receive_batch(socket, 0, args[1]);
    } else {
        fprintf(stderr, "INVALID COMMAND \"%s\" SUPPLIED!\n", args[0]);
    }
    return 0;
}

int batch_backup(int socket, char *regex)
{
    char buf[1024];
    memset(buf, 0, 1024 * sizeof *buf);
    unsigned int filecount = 0;
    if (regex[0] == '*') {
        buf[0] = '.';
        strncpy(buf + 1, regex, strlen(regex));
    } else {
        strncpy(buf, regex, strlen(regex));
    }
    char **f = get_filenames(buf, &filecount);
    if (!f || filecount == 0) {
        printf("NO MATCHES...\n");
        return errno;
    }
    printf("MATCHES: ");
    for (unsigned int i = 0; i < filecount; ++i)
        printf("'%s' ", f[i]);
    printf("\n");
    int res = send_batch(socket, BACKUP_BATCH, BACKUP_FILE, f, filecount);
    // eh isso aq ctz
    for (unsigned int i = 0; i < filecount; ++i)
        free(f[i]);
    free(f);
    return res;
}


int check_md5(int socket, char *filename)
{
    unsigned char *md5_client, *md5_server;
    msg_t *response = NULL;
    md5_client = calc_md5sum(filename);
    if (!md5_client) {
        fprintf(stderr, "COULD NOT CALCULATE MD5 FOR FILE \"%s\"\n", filename);
        return -1;
    }
    // validate file exists here: calc md5
    // get md5 from server
    // compare :)
    response = send_and_wait(socket, VERIFY, filename, strlen(filename), RECV_MD5);
    if (!response) {
        // TODO: checar erros direito
        fprintf(stderr, "SERVER DID NOT SEND MD5 FOR FILE \"%s\"\n", filename);
        return errno;
    }
    md5_server = (unsigned char*)response->data;
    printf("> client: ");
    print_md5(md5_client);
    printf("> server: ");
    print_md5(md5_server);
    for (unsigned int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        if (md5_client[i] != md5_server[i]) {
            fprintf(stderr, "MD5s DID NOT MATCH!\n");
            break;
        } else if (i == MD5_DIGEST_LENGTH - 1) {
            printf("> FILES MATCH!\n");
        }
    }
    free(md5_client);
    destroy_message(response);
    return 0;
}

int list_subdirectories(char *relative_path)
{
    if (!relative_path)
        relative_path = ".";
    DIR *dp;
    struct dirent *ep;
    dp = opendir(relative_path);
    if (!dp)
        return -1;
    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.')
            continue;
        printf("%s%s\n", ep->d_name, ep->d_type == 4 ? "/" : "");
    }
    closedir(dp);
    return 0;
}

int change_directory(char *originalcwd, char *cwd, char *relative_path)
{
    int status = 0;
    if (!strcmp(originalcwd, cwd))
        return -5;
    status = chdir(relative_path);
    if (!status) {
        // TODO: mudar cwd
    }
    return status;
}
