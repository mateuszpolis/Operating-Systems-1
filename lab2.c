#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t sig_count = 0;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sig_handler(int sig)
{
  sig_count++;
  printf("Signal received %d times\n", sig_count);
  if (sig_count == 100) {
    if (kill(0, SIGUSR2) < 0)
      ERR("kill");
  }
  sethandler(sig_handler, SIGUSR1);
}

void sigchld_handler(int sig)
{
    exit(EXIT_SUCCESS);
}

void child_work()
{
    srand(time(NULL) * getpid());
    int t = 100 + rand() % (200 - 100 + 1);
    struct timespec ts = {0, t * 1000000L};
    int i;
    for (i = 0; i < 30; i++) {
      nanosleep(&ts, NULL);
      if (kill(getppid(), SIGUSR1) < 0)
        ERR("kill");
    }
}

void create_children(int n)
{
    pid_t s;
    for (n--; n >= 0; n--)
    {
        if ((s = fork()) < 0)
            ERR("Fork:");
        if (!s)
        {
            sethandler(SIG_IGN, SIGUSR1);
            sethandler(sigchld_handler, SIGUSR2);
            child_work();
            exit(EXIT_SUCCESS);
        }
    }
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
    sethandler(sig_handler, SIGUSR1);
    create_children(n);
    pid_t pid;
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
    }
    return EXIT_SUCCESS;
}