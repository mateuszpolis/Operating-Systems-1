#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#define MAX_PATH 101

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define FILE_BUF_LEN 256

int sum_size = 0;

void scan_dir(char *path)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat filestat;
    if (NULL == (dirp = opendir(path))) {
        if (errno == ENOENT) {
                printf("Directory %s does not exist.\n", path);
        } else if (errno == EACCES) {
                printf("Directory %s not accessible.\n", path);
        } else {
                ERR("chdir");
        }
    }
    do
    {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL)
        {
            if (lstat(dp->d_name, &filestat))
                ERR("lstat");
            if (S_ISDIR(filestat.st_mode))
                continue;
            sum_size += filestat.st_size;
        }
    } while (dp != NULL);

    if (errno != 0)
        ERR("readdir");
    if (closedir(dirp))
        ERR("closedir");
}

int main(int argc, char **argv)
{
    int i;
    char path[MAX_PATH];
    if (getcwd(path, MAX_PATH) == NULL)
        ERR("getcwd");
    const int fd_1 = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd_1 == -1) {
        ERR("open");
    }
    for (i = 1; i < argc; i+=2)
    {
        if (chdir(argv[i])) {
                if (errno == ENOENT) {
                        printf("Directory %s does not exist.\n", argv[i]);
                        continue;
                } else if (errno == EACCES) {
                        printf("Directory %s not accessible.\n", argv[i]);
                        continue;
                } else {
                        ERR("chdir");
                }
        }
        scan_dir(argv[i]);
        if (strtoull(argv[i+1],NULL,10) < sum_size) {
                if (write(fd_1, argv[i], strlen(argv[i])) == -1)
                        ERR("write");
                if (write(fd_1, " ", 1) == -1)
                        ERR("write");
        }
        sum_size = 0;
        if (chdir(path))
            ERR("chdir");
    }
    if (close(fd_1) == -1)
            ERR("close");
    return EXIT_SUCCESS;
}
