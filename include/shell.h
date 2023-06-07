#ifndef __REDES_T1_SHELL_H
#define __REDES_T1_SHELL_H

#include "protocol.h"

#define PS1 "> "

void shell_rpel(int socket);

/* --- shell interface --- */
void print_ps1(char *pwd);

void print_error(char *description);

/* --- shell utils --- */
char *shell_read_line(void);

char **shell_split_line(char *);

/* --- shell commands --- */
int exec_command(int socket, char **args, char *ocwd, char *cwd);

int list_subdirectories(char *relative_path);

int change_directory(char *ocwd, char *cwd, char *relative_path);

void command_loop(int socket, int ending_type, msg_t **first_msg, FILE **fp, int is_sender);

int check_md5(int socket, char *filename);

int batch_backup(int socket, char *regex);

#endif /* __REDES_T1_SHELL_H */
