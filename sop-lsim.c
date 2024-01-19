#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define UNUSED(x) (void)(x)

volatile sig_atomic_t last_signal = 0;
volatile sig_atomic_t received2 = 0;

void sethandler(void(*f), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_flags = SA_SIGINFO;
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sig_handler(int sig, siginfo_t *siginfo)
{
    if (sig == SIGUSR1)
    {
        if (kill(siginfo->si_pid, SIGUSR2) < 0)
            ERR("kill");
        printf("Teacher has accepted solution of student [%d].\n", siginfo->si_pid);
    }
}

void sigchild_handler(int sig)
{
    if (sig == SIGUSR2)
        received2 = 1;
}

int child_work(int prob, int no, int t, int parts)
{
    printf("Student [%d, %d] has started doing task!\n", no, getpid());
    srand(time(NULL) * getpid());
    int i;
    int issues = 0;
    for (i = 0; i < parts; i++)
    {
        printf("Student [%d, %d] is starting doing part %d of %d\n", no, getpid(), i + 1, parts);
        int j;
        for (j = 0; j < t; j++)
        {
            struct timespec time = {0, 1000000};
            nanosleep(&time, NULL);
            int r = rand() % 101;
            if (r <= prob)
            {
                printf("Student [%d, %d] has issue (%d) doing task!\n", no, getpid(), ++issues);
                struct timespec tissue = {0, 500000};
                nanosleep(&tissue, NULL);
            }
        }
        received2 = 0;
        while (1)
        {
            if (kill(getppid(), SIGUSR1) < 0)
                ERR("kill");
            struct timespec thand = {0, 100000};
            nanosleep(&thand, NULL);
            if (received2 == 1)
            {
                printf("Student [%d, %d] has finished part %d of %d\n", no, getpid(), i + 1, parts);
                break;
            }
        }
    }

    return issues;
}

void create_children(int n, char *args[], int t, int p)
{
    int i;
    for (i = 3; i < n; i++)
    {
        switch (fork())
        {
            case 0:
                sethandler(sigchild_handler, SIGUSR2);
                int issues = child_work(atoi(args[i]), i - 2, t, p);
                exit(issues);
            case -1:
                perror("Fork:");
                exit(EXIT_FAILURE);
        }
    }
}

void usage(const char *pname)
{
    fprintf(stderr, "USAGE: %s p t  p1 p2 ... pn\n", pname);  // TODO
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    UNUSED(argc);
    if (argc < 3)
        usage(argv[0]);
    int p, t;
    p = atoi(argv[1]);
    t = atoi(argv[2]);
    sethandler(sig_handler, SIGUSR1);
    create_children(argc, argv, t, p);

    int n = argc - 3;
    int columns = 3;
    int **values = (int **)malloc(n * sizeof(int *));
    if (values == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; i++)
    {
        values[i] = (int *)malloc(columns * sizeof(int));
        if (values[i] == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
    }

    pid_t pid;
    int j = 0;
    int total_issues = 0;
    for (;;)
    {
        int status;
        pid = waitpid(0, &status, WNOHANG);
        if (pid == 0)
            continue;
        if (pid < 0)
        {
            if (errno == ECHILD)
                break;
            ERR("waitpid:");
        }
        if (pid > 0 && status)
        {
            values[j][0] = j + 1;
            values[j][1] = pid;
            values[j][2] = WEXITSTATUS(status);
            j++;
            total_issues += WEXITSTATUS(status);
        }
    }

    int it;
    for (it = 0; it < n; it++)
    {
        printf("%2d|%4d|%2d\n", values[it][0], values[it][1], values[it][2]);
    }
    printf("Total issues: %d\n", total_issues);

    exit(EXIT_SUCCESS);
}
