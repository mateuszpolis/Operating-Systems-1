#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))
#define PIPE_READ 0
#define PIPE_WRITE 1

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
  ({ __typeof__(expression) __result; \
     do __result = (expression); while (__result == -1 && errno == EINTR); \
     __result; })
#endif

volatile sig_atomic_t sig_count = 0;

int compare_k ( const void *pa, const void *pb ) {
    const int *a = *(const int **)pa;
    const int *b = *(const int **)pb;
    return a[1] - b[1];
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sigchild_handler(int sig)
{
    sig_count++;
}

void child_work(int i, int pipe_fd)
{
    srand(time(NULL) * getpid());
    int k = rand() % 11;
    int s = 0;
    while (s < 10)
    {
        if (s < k)
        {
            if (kill(0, SIGUSR1) < 0)
                ERR("kill");
        }
        s++;
        sleep(1);
    }
    int values[3] = {getpid(), k, sig_count};
    if (write(pipe_fd, values, sizeof(values)) < 0)
        ERR("write");
}

void create_children(int n, int pipe_fd)
{
    pid_t s;
    for (n--; n >= 0; n--)
    {
        if ((s = fork()) < 0)
            ERR("Fork:");
        if (!s)
        {
            struct sigaction act;
            memset(&act, 0, sizeof(struct sigaction));
            act.sa_handler = sigchild_handler;
            if (-1 == sigaction(SIGUSR1, &act, NULL))
                ERR("sigaction");
            child_work(n, pipe_fd);
            exit(EXIT_SUCCESS);
        }
    }
}

ssize_t bulk_write(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = write(fd, buf, count);
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s 0<n\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int n;
    if (argc < 2)
        usage(argv[0]);
    n = atoi(argv[1]);
    if (n <= 0)
        usage(argv[0]);

    int pipe_fd[2];
    if (pipe(pipe_fd) < 0)
        ERR("pipe");

    sethandler(SIG_IGN, SIGUSR1);
    create_children(n, pipe_fd[PIPE_WRITE]);

    close(pipe_fd[PIPE_WRITE]);

    int columns = 3;
    int **values = (int **)malloc(n * sizeof(int *));
    if (values == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; i++) {
        values[i] = (int *)malloc(columns * sizeof(int));
        if (values[i] == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }
    pid_t pid;

    int i = 0;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            continue;
        if (pid < 0)
        {
            if (errno == ECHILD)
                break;
            ERR("waitpid:");
        }
        if (read(pipe_fd[PIPE_READ], values[i], sizeof(int) * columns) < 0)
            ERR("read");
        i++;
    }
        
    qsort(values, n, sizeof values[0], compare_k);

    int out;
    if ((out = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0777)) < 0)
      ERR("open");

    for (i = 0; i < n; i++) {
        char buf[50];
        snprintf(buf, sizeof(buf), "(%d, %d, %d)\n", values[i][0], values[i][1], values[i][2]);
        bulk_write(out, buf, strlen(buf));
    }

    close(pipe_fd[PIPE_READ]);
    for (int i = 0; i < n; i++) {
        free(values[i]);
    }
    free(values);
    return EXIT_SUCCESS;
}
