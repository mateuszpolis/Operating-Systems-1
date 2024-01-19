#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define PATH_BUF_LEN 256
#define TEXT_BUF_LEN 256
#define MAXFD 20

ssize_t bulk_read(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void write_stage2(const char *const path, const struct stat *const state_buff)
{
    if (!S_ISREG(state_buff->st_mode))
    {
        ERR("not regular file");
    }
    remove(path);
    FILE *fptr;
    if ((fptr = fopen(path, "w+")) == NULL)
    {
        ERR("fopen");
    };
    char input[TEXT_BUF_LEN];
    while ((fgets(input, TEXT_BUF_LEN, stdin)) != NULL && (input[0] != '\n'))
    {
        if (input[0] == '\0')
        {
            break;
        }
        int i = 0;
        while (input[i] != '\0')
        {
            fprintf(fptr, "%c", toupper(input[i]));
            i++;
        }
    }
    fclose(fptr);
    return;
}

void show_stage3(const char *const path, const struct stat *const state_buff)
{
    if (S_ISDIR(state_buff->st_mode))
    {
        DIR *dirp;
        struct dirent *dp;
        if (NULL == (dirp = opendir(path)))
            ERR("opendir");
        do
        {
            errno = 0;
            if ((dp = readdir(dirp)) != NULL)
            {
                printf("%s\n", dp->d_name);
            }
        } while (dp != NULL);

        if (errno != 0)
            ERR("readdir");
        if (closedir(dirp))
            ERR("closedir");
    }
    else if (S_ISREG(state_buff->st_mode))
    {
        int fd;
        if (path == NULL)
        {
            return;
        }
        fd = open(path, O_RDONLY);
        if (fd == -1)
        {
            ERR("open");
        }
        char buff[TEXT_BUF_LEN];
        int size;
        size = bulk_read(fd, buff, TEXT_BUF_LEN);
        printf("%s\n", buff);
        printf("%d\n", size);
        printf("%d\n", state_buff->st_uid);
        printf("%d\n", state_buff->st_gid);
        close(fd);
    }
}

int walk(const char *name, const struct stat *, int type, struct FTW *f)
{
    switch (type)
    {
        case FTW_DNR:
        case FTW_D:
            for (int i = 0; i < f->level; i++)
            {
                printf(" ");
            }
            printf("+%s\n", name);
            break;
        case FTW_F:
            for (int i = 0; i < f->level + 1; i++)
            {
                printf(" ");
            }
            printf("%s\n", name);
            break;
        default:
            break;
    }
    return 0;
}

void walk_stage4(const char *const path, const struct stat *const)
{
    if (nftw(path, walk, MAXFD, FTW_PHYS) == 0)
        return;
}

int interface_stage1()
{
    printf("A. write\nB. show\nC. walk\nD. exit\n");
    char command;
    command = fgetc(stdin);
    while (getchar() != '\n')
        ;  // Clearing the buffer
    if (command == 'D' || command == 'd')
    {
        return 0;
    }
    char path[PATH_BUF_LEN];
    struct stat filestat;
    fgets(path, PATH_BUF_LEN, stdin);
    path[strcspn(path, "\n")] = 0;
    if (lstat(path, &filestat))
    {
        ERR("invalid path");
        return 1;
    }
    switch (command)
    {
        case 'A':
        case 'a':
            write_stage2(path, &filestat);
            return 1;
            break;
        case 'B':
        case 'b':
            show_stage3(path, &filestat);
            return 1;
            break;
        case 'C':
        case 'c':
            walk_stage4(path, &filestat);
            return 1;
            break;
        case 'D':
        case 'd':
            return 0;
            break;
        default:
            ERR("invalid command");
            return 1;
            break;
    }
}

int main()
{
    while (interface_stage1())
        ;
    return EXIT_SUCCESS;
}
