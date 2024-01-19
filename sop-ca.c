#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
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

typedef struct timespec timespec_t;
typedef unsigned int UINT;
typedef struct argsThread
{
    pthread_t tid;
    UINT seed;
    int *array;
    pthread_mutex_t *pmxArray;
    int n;
    int p;
    sigset_t *mask;
    int *nOfThreads;
    pthread_mutex_t *pmxNOfThreads;
    pthread_mutex_t *pmxWholeArray;

} argsThread_t;

void readArguments(int argc, char **argv, int *n, int *p);
void *printArray(void *voidArgs);
void signalHandler(argsThread_t *args);
void performInversion(void *voidArgs);
void msleep(UINT milisec);
void createThread(argsThread_t *args, void *(f));

void usage(const char *pname)
{
    fprintf(stderr, "USAGE: %s n p\n", pname);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int n, p;
    readArguments(argc, argv, &n, &p);
    int *array = (int *)malloc(sizeof(int) * n);
    if (array == NULL)
    {
        ERR("Malloc error");
    }
    for (int i = 0; i < n; i++)
    {
        array[i] = i;
    }

    pthread_mutex_t *mxArray = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * n);
    if (mxArray == NULL)
        ERR("Malloc error");

    for (int i = 0; i < n; i++)
    {
        pthread_mutex_init(&mxArray[i], NULL);
    }

    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGUSR1);
    sigaddset(&newMask, SIGUSR2);
    sigaddset(&newMask, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    argsThread_t *argsArray = (argsThread_t *)malloc(p * sizeof(argsThread_t));
    if (argsArray == NULL)
        ERR("Malloc error");
    srand(time(NULL));
    int nOfThreads = 0;
    pthread_mutex_t mxNOfThreads = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t mxWholeArray = PTHREAD_MUTEX_INITIALIZER;
    for (int i = 0; i < p; i++)
    {
        argsArray[i].seed = (UINT)rand();
        argsArray[i].array = array;
        argsArray[i].pmxArray = mxArray;
        argsArray[i].n = n;
        argsArray[i].p = p;
        argsArray[i].mask = &newMask;
        argsArray[i].nOfThreads = &nOfThreads;
        argsArray[i].pmxNOfThreads = &mxNOfThreads;
        argsArray[i].pmxWholeArray = &mxWholeArray;
    }

    signalHandler(argsArray);

    for (int i = 0; i < p; i++)
    {
        if (pthread_join(argsArray[i].tid, NULL))
            ERR("Couldn't join thread");
    }

    if (pthread_sigmask(SIG_UNBLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    free(array);
    free(mxArray);
    exit(EXIT_SUCCESS);
}

void readArguments(int argc, char **argv, int *n, int *p)
{
    if (argc >= 2)
    {
        *n = atoi(argv[1]);
        if (*n < 8 || *n > 256)
        {
            printf("'n' must be in range [8, 256]");
            exit(EXIT_FAILURE);
        }
    }
    if (argc >= 3)
    {
        *p = atoi(argv[2]);
        if (*p < 1 || *p > 16)
        {
            printf("'m' must be in range [1, 16]");
            exit(EXIT_FAILURE);
        }
    }
}

void signalHandler(argsThread_t *args)
{
    int signo;
    int quit = 0;
    for (;;)
    {
        if (sigwait(args->mask, &signo))
            ERR("sigwait failed.");
        switch (signo)
        {
            case SIGUSR1:
                createThread(&args[*args->nOfThreads], performInversion);
                break;
            case SIGUSR2:
                createThread(&args[*args->nOfThreads], printArray);
                break;
            case SIGINT:
                printf("Program terminating\n");
                quit = 1;
                break;
            default:
                printf("unexpected signal %d\n", signo);
                exit(EXIT_FAILURE);
        }

        if (quit)
        {
            break;
        }
    }
}

void createThread(argsThread_t *args, void *(f))
{
    pthread_attr_t threadAttr;
    if (pthread_attr_init(&threadAttr))
        ERR("Couldn't initialize thread attributes");
    pthread_mutex_lock(args->pmxNOfThreads);
    if (*args->nOfThreads == args->p)
    {
        printf("All thread busy, aborting request and return to waiting.\n");
        pthread_mutex_unlock(args->pmxNOfThreads);
    }
    else
    {
        args->nOfThreads = args->nOfThreads + 1;
        pthread_mutex_unlock(args->pmxNOfThreads);
        if (pthread_create(&args->tid, &threadAttr, f, args))
            ERR("Couldn't create thread");
        if (pthread_attr_destroy(&threadAttr))
            ERR("Couldn't destroy thread attributes");
    }
}

void performInversion(void *voidArgs)
{
    argsThread_t *args = (argsThread_t *)voidArgs;
    int a = (int)rand_r(&args->seed) % (args->n - 1);
    int b = (int)rand_r(&args->seed) % (args->n - a - 1) + a + 1;
    pthread_mutex_lock(args->pmxWholeArray);
    while (a < b)
    {
        pthread_mutex_lock(&args->pmxArray[a]);
        pthread_mutex_lock(&args->pmxArray[b]);
        int temp = args->array[b];
        args->array[b] = args->array[a];
        args->array[a] = temp;
        pthread_mutex_unlock(&args->pmxArray[a]);
        pthread_mutex_unlock(&args->pmxArray[b]);
        a += 1;
        b -= 1;
        msleep(5);
    }
    pthread_mutex_unlock(args->pmxWholeArray);
    pthread_mutex_lock(args->pmxNOfThreads);
    args->nOfThreads = args->nOfThreads - 1;
    pthread_mutex_unlock(args->pmxNOfThreads);
}

void *printArray(void *voidArgs)
{
    argsThread_t *args = (argsThread_t *)voidArgs;

    pthread_mutex_lock(args->pmxWholeArray);
    printf("[ ");
    for (int i = 0; i < args->n; i++)
    {
        printf("%d ", args->array[i]);
    }
    printf("]\n");
    pthread_mutex_unlock(args->pmxWholeArray);
    pthread_mutex_lock(args->pmxNOfThreads);
    args->nOfThreads = args->nOfThreads - 1;
    pthread_mutex_unlock(args->pmxNOfThreads);

    return NULL;
}

void msleep(UINT milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    timespec_t req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}
