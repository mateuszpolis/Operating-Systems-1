#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<math.h>

#define MAXLINE 4096
#define DEFAULT_N 3
#define DEFAULT_K 7
#define NEXT_DOUBLE(seedptr) ((double)rand_r(seedptr) / (double)RAND_MAX)
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef unsigned int UINT;
typedef struct timespec timespec_t;
typedef struct argsThread
{
    pthread_t tid;
    UINT seed;
    int *taskArray;
    pthread_mutex_t *pmxResultArray;
    double *resultArray;
    int arraySize;
    int *toCalculate;
    pthread_mutex_t *pmxToCalculate;
} argsThread_t;

void ReadArguments(int argc, char **argv, int *numberOfThreads, int *taskArraySize);
void make_threads(argsThread_t *argsArray, int numberOfThreads);
void *threadFunction(void *args);
void fillArray(int *array, int size);
void printIntArray(int *array, int size);
void printDoubleArray(double *array, int size);
void msleep(UINT milisec);

int main(int argc, char **argv)
{
    int n, k;
    ReadArguments(argc, argv, &n, &k);

    pthread_mutex_t *mxResultArray = (pthread_mutex_t *)malloc(k * sizeof(pthread_mutex_t));
    if (mxResultArray == NULL)
        ERR("Malloc error");
    for (int i = 0; i < k; i++)
    {
        int err = pthread_mutex_init(&mxResultArray[i], NULL);
        if (err != 0)
            ERR("Couldn't initialize mutex");
    }
    int *taskArray = (int *)malloc(k * sizeof(int));
    if (taskArray == NULL)
        ERR("Malloc error");
    double *resultArray = (double *)malloc(k * sizeof(double));
    if (resultArray == NULL)
        ERR("Malloc error");
    srand(time(NULL));
    fillArray(taskArray, k);
    for (int i = 0; i < k; i++)
        resultArray[i] = 0;
  
    argsThread_t *argsArray = (argsThread_t *)malloc((n + 1) * sizeof(argsThread_t));
    if (argsArray == NULL)
        ERR("Malloc error");
    pthread_mutex_t mxToCalculate = PTHREAD_MUTEX_INITIALIZER;
    int toCalculate = k;
    for (int i = 0; i < n + 1; i++)
    {
        argsArray[i].pmxResultArray = mxResultArray;
        argsArray[i].taskArray = taskArray;
        argsArray[i].resultArray = resultArray;
        argsArray[i].arraySize = k;
        argsArray[i].seed = (UINT)rand();
        argsArray[i].toCalculate = &toCalculate;
        argsArray[i].pmxToCalculate = &mxToCalculate;
    }
    make_threads(argsArray, n);
    for (int i = 0; i < n + 1; i++)
    {
        int err = pthread_join(argsArray[i].tid, NULL);
        if (err != 0)
            ERR("Couldn't join thread");
    }
    printIntArray(taskArray, k);
    printDoubleArray(resultArray, k);
    free(argsArray);
    free(taskArray);
    free(resultArray);
    free(mxResultArray);
    exit(EXIT_SUCCESS);
}

void ReadArguments(int argc, char **argv, int *numberOfThreads, int *taskArraySize)
{
    *numberOfThreads = DEFAULT_N;
    *taskArraySize = DEFAULT_K;
    if (argc >= 2)
    {
        *numberOfThreads = atoi(argv[1]);
        if (*numberOfThreads < 1)
        {
            printf("Invalid value for 'number of threads'");
            exit(EXIT_FAILURE);
        }
    }
    if (argc >= 3)
    {
        *taskArraySize = atoi(argv[2]);
        if (*taskArraySize < 1)
        {
            printf("Invalid value for 'task array size'");
            exit(EXIT_FAILURE);
        }
    }
}

void make_threads(argsThread_t *argsArray, int numberOfThreads)
{
    for (int i = 0; i < numberOfThreads + 1; i++)
    {
      int err = pthread_create(&(argsArray[i].tid), NULL, threadFunction, &argsArray[i]);
      if (err != 0)
        ERR("Couldn't create thread");
    }
}

void *threadFunction(void *voidPtr)
{
    argsThread_t *args = (argsThread_t *)voidPtr;
    int randomIndex;
    double result;
    while (*(args->toCalculate) > 0)
    {
        randomIndex = (int)rand_r(&args->seed) % args->arraySize;
        pthread_mutex_lock(&args->pmxResultArray[randomIndex]);
        if (args->resultArray[randomIndex] == 0) {
            result = sqrt(args->taskArray[randomIndex]);
            args->resultArray[randomIndex] = result;
            pthread_mutex_lock(args->pmxToCalculate);
            (*(args->toCalculate))--;
            pthread_mutex_unlock(args->pmxToCalculate);    
        }
        pthread_mutex_unlock(&args->pmxResultArray[randomIndex]);
        msleep(100);
    }
    return NULL;
}

void printIntArray(int *array, int size)
{
    printf("[ ");
    for (int i = 0; i < size; i++)
        printf("%d ", array[i]);
    printf("]\n");
}

void printDoubleArray(double *array, int size)
{
    printf("[ ");
    for (int i = 0; i < size; i++)
        printf("%f ", array[i]);
    printf("]\n");
}

void fillArray(int *array, int size) {
    for (int i = 0; i < size; i++) {
        int random = (rand() % 60) + 1;
        array[i] = random;
    }
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
