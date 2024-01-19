#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define DEFAULT_N 4
#define DEFAULT_M 5
#define DEFAULT_K 10
#define MAX_INPUT 20
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef unsigned int UINT;
typedef struct argsThread
{
    pthread_t tid;
    UINT seed;
    char **characterArray;
    int n;
    int m;
    int k;
    pthread_mutex_t **pmxCharacterArray;    
    sigset_t *mask;
} argsThread_t;

void readArguments(int argc, char **argv, int *n, int *m, int *k);
void fillArray(char **characterArray, int n, int m, UINT *seedptr);
void makeThreads(argsThread_t *argsArray);
void *threadFunction(void *voidArgs);
void replaceItem(argsThread_t *args);
void readCommands(argsThread_t *args);
void printArray(char **characterArray, int n, int m); 

int main(int argc, char **argv)
{
    int n, m, k;
    readArguments(argc, argv, &n, &m, &k);
    char **characterArray = malloc(n * sizeof(char *));
    if (characterArray == NULL)
        ERR("Malloc error");
    for (int i = 0; i < n; i++) { 
        characterArray[i] = malloc(m * sizeof(char));
        if (characterArray[i] == NULL)
            ERR("Malloc error");
    }
    srand(time(NULL));
    UINT seed = rand();
    fillArray(characterArray, n, m, &seed);

    pthread_mutex_t **mxCharacterArray = malloc(n * sizeof(pthread_mutex_t *));
    if (mxCharacterArray == NULL)
        ERR("Malloc error");
    for (int i = 0; i < n; i++) {
        mxCharacterArray[i] = malloc(m * sizeof(pthread_mutex_t));
        if (mxCharacterArray[i] == NULL)
            ERR("Malloc error");
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; ++j) {
            if (pthread_mutex_init(&mxCharacterArray[i][j], NULL))
                ERR("Couldn't initialize mutex");
        }
    }

    sigset_t oldMask, newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGUSR1);
    sigaddset(&newMask, SIGINT);
    if (pthread_sigmask(SIG_BLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    argsThread_t *argsArray = malloc(k * sizeof(argsThread_t));
    if (argsArray == NULL)
        ERR("Malloc error");
    for (int i = 0; i < k; i++)
    {
        argsArray[i].seed = (UINT)rand();
        argsArray[i].characterArray = characterArray;
        argsArray[i].pmxCharacterArray = mxCharacterArray;
        argsArray[i].mask = &newMask;
        argsArray[i].n = n;
        argsArray[i].m = m;
        argsArray[i].k = k;
    }
    makeThreads(argsArray);
    
    readCommands(argsArray);
    
    for (int i = 0; i < k; i++)
    {
        if (pthread_join(argsArray[i].tid, NULL))
            ERR("Couldn't join thread");
    }

    if (pthread_sigmask(SIG_UNBLOCK, &newMask, &oldMask))
        ERR("SIG_BLOCK error");

    for (int i = 0; i < n; i++) {
      free(characterArray[i]);
      free(mxCharacterArray[i]);
    }
    free(characterArray);
    free(mxCharacterArray);
    free(argsArray);

    exit(EXIT_SUCCESS);
}

void readArguments(int argc, char **argv, int *n, int *m, int *k)
{
    *n = DEFAULT_N;
    *m = DEFAULT_M;
    *k = DEFAULT_K;
    if (argc >= 2)
    {
        *n = atoi(argv[1]);
        if (*n < 1 || *n > 100)
        {
            printf("'n' must be in range [1, 100]");
            exit(EXIT_FAILURE);
        }
    }
    if (argc >= 3)
    {
        *m = atoi(argv[2]);
        if (*m < 1 || *m > 100)
        {
            printf("'m' must be in range [1, 100]");
            exit(EXIT_FAILURE);
        }
    }

    if (argc >= 4)
    {
        *k = atoi(argv[3]);
        if (*k < 3 || *k > 100)
        {
            printf("'k' must be in range [3, 100]");
            exit(EXIT_FAILURE);
        }
    }
}

void fillArray(char **characterArray, int n, int m, UINT *seedptr) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < m; j++) {
      char randomChar = 'a' + (rand_r(seedptr) % 26);
      characterArray[i][j] = randomChar;
    }
  }
}

void makeThreads(argsThread_t *argsArray) {
    pthread_attr_t threadAttr;
    if (pthread_attr_init(&threadAttr))
        ERR("Couldn't initialize thread attributes");
    for (int i = 0; i < argsArray->k; i++)
    {
        if (pthread_create(&argsArray[i].tid, &threadAttr, threadFunction, &argsArray[i]))
            ERR("Couldn't create thread");
    }
    if (pthread_attr_destroy(&threadAttr))
        ERR("Couldn't destroy thread attributes");
}

void *threadFunction(void *voidArgs) {
    argsThread_t *args = voidArgs;

    int signo;
    for (;;)
    {
        if (sigwait(args->mask, &signo))
            ERR("sigwait failed.");
        switch (signo)
        {
            case SIGUSR1:
                replaceItem(voidArgs);
                break;
            case SIGINT:
                printf("[TID %lu] Received SIGINT: Exiting\n", (unsigned long)pthread_self());
                return NULL;
            default:
                printf("unexpected signal %d\n", signo);
                exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

void replaceItem(argsThread_t *args) {
    int x = rand_r(&args->seed) % args->n;
    int y = rand_r(&args->seed) % args->m;
    pthread_mutex_lock(&args->pmxCharacterArray[x][y]);
    char previousChar = args->characterArray[x][y];
    char newChar = 'a' + (rand_r(&args->seed) % 26);
    args->characterArray[x][y] = newChar;
    pthread_mutex_unlock(&args->pmxCharacterArray[x][y]);
    printf("[TID %lu] Received SIGUSR1: Replacing '%c' with '%c' at [%d, %d]\n", (unsigned long)pthread_self(), previousChar, newChar, x, y);
}

void readCommands(argsThread_t *args) {
    char command[MAX_INPUT];
    while (1) {
        fgets(command, MAX_INPUT, stdin);
        if (strcmp(command, "print\n") == 0) {
          printArray(args->characterArray, args->n, args->m);
        } else if (strncmp(command, "replace ", 8) == 0) {
          int nOfSignals = atoi(command + 8);
          for (int i = 0; i < nOfSignals; i++) {
            kill(getpid(), SIGUSR1);
          }
        } else if (strcmp(command, "exit\n") == 0) {
          for (int i = 0; i < args->k; i++) {
            pthread_kill(args[i].tid, SIGINT);
          }
          break;
        } else {
          ERR("Invalid command");
        }
    }
}

void printArray(char **characterArray, int n, int m) {
  printf("\nArray:\n");
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < m; ++j) {
      printf("%c", characterArray[i][j]);
    }
    printf("\n");
  }
}
