#ifndef __REDES_T1_UTILS_H
#define __REDES_T1_UTILS_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <dirent.h>
#include <errno.h>
#include <regex.h>
#include <string.h>

#define MAX_FILE_PATH (MAX_DATA_BYTES)
#define MD5BUF 2048

int file_exists(char *file_path)
{
    return access(file_path, F_OK) == 0;
}

char **get_filenames(char *regex, unsigned int *count)
{
    DIR *dp;
    struct dirent *ep;
    char **filenames = malloc(1024 * sizeof *filenames);
    regex_t rg;
    int a;
    char b[1024];
    *count = 0;

    if (!filenames)
        return NULL;

    if ((a = regcomp(&rg, regex, REG_EXTENDED))) {
        regerror(a, &rg, b, 1023);
        printf("%s\n", b);
        errno = EINVAL;
        return NULL;
    }

    dp = opendir(".");
    if (!dp) {
        free(filenames);
        return NULL;
    }
    printf("REGEX: %s\n", regex);

    while ((ep = readdir(dp))) {
        if (ep->d_name[0] == '.')
            continue;
        switch (regexec(&rg, ep->d_name, 0, NULL, 0)) {
            case 0:
                filenames[*count] = malloc(sizeof *filenames[*count] * (strlen(ep->d_name) + 1));
                memset(filenames[*count], 0, sizeof *filenames[*count] * (strlen(ep->d_name) + 1));
                ++*count;
                if (!filenames[*count - 1]) {
                    for (unsigned int j = 0; j < *count; ++j)
                        free(filenames[j]);
                    free(filenames);
                    *count = 0;
                    return NULL;
                }
                strncpy(filenames[*count - 1], ep->d_name, strlen(ep->d_name));
                break;
            case REG_NOMATCH:
                break;
            default:
                printf("???????????\n");
                break;
        }
    }

    // regfree(&rg);
    closedir(dp);
    return filenames;
}

unsigned char *calc_md5sum(char *filename)
{
    FILE *fp;
    MD5_CTX md_context;
    int count = 0;
    unsigned char buf[MD5BUF];
    unsigned char *md5 = malloc(MD5_DIGEST_LENGTH * sizeof *md5);
    if (!md5)
        return NULL;
    memset(md5, 0, MD5_DIGEST_LENGTH * sizeof *md5);
    memset(buf, 0, MD5BUF * sizeof *buf);
    fp = fopen(filename, "rb");
    if (!fp)
        return NULL;
    MD5_Init(&md_context);
    while ((count = fread(buf, sizeof *buf, MD5BUF, fp)))
        MD5_Update(&md_context, buf, count);
    MD5_Final(md5, &md_context);
    fclose (fp);
    return md5;;
}

void print_md5(unsigned char *md5)
{
    for (unsigned int i = 0; i < MD5_DIGEST_LENGTH; ++i)
        printf("%02x", md5[i]);
    printf("\n");
}

unsigned char errno_to_code(int errnum)
{
    return (unsigned char)errnum;
}
#endif /* __REDES_T1_UTILS_H */
