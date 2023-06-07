#ifndef __REDES_T1_UTILS_H
#define __REDES_T1_UTILS_H

#define MAX_FILE_PATH 63

int file_exists(char *file_path);

char **get_filenames(char *regex, unsigned int *count);

unsigned char *calc_md5sum(char *filename);

unsigned char errno_to_code(int errnum);

void print_md5(unsigned char *md5);

#endif /* __REDES_T1_UTILS_H */
