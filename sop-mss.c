#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define UNUSED(x) ((void)(x))

#define DECK_SIZE (4 * 13)
#define HAND_SIZE (7)
#define N 4

volatile sig_atomic_t last_signal = 0;

void set_handler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sigusr_handler(int sig) { last_signal = sig; }

typedef struct
{
    int id;
    int *deck;
    unsigned int seed;
    int n;
    int *givenCards;
    pthread_barrier_t *barrier;
    sem_t *semaphore;
} thread_arg;

void usage(const char *program_name)
{
    fprintf(stderr, "USAGE: %s n\n", program_name);
    exit(EXIT_FAILURE);
}

void shuffle(int *array, size_t n)
{
    if (n > 1)
    {
        size_t i;
        for (i = 0; i < n - 1; i++)
        {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

void print_deck(const int *deck, int size)
{
    const char *suits[] = {" of Hearts", " of Diamonds", " of Clubs", " of Spades"};
    const char *values[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King", "Ace"};

    char buffer[1024];
    int offset = 0;

    if (size < 1 || size > DECK_SIZE)
        return;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[");
    for (int i = 0; i < size; ++i)
    {
        int card = deck[i];
        if (card < 0 || card > DECK_SIZE)
            return;
        int suit = deck[i] % 4;
        int value = deck[i] / 4;

        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s", values[value]);
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s", suits[suit]);
        if (i < size - 1)
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, ", ");
    }
    snprintf(buffer + offset, sizeof(buffer) - offset, "]");

    puts(buffer);
}

void ReadArguments(int argc, char **argv, int *n)
{
    *n = N;

    if (argc >= 2)
    {
        *n = atoi(argv[1]);
        if (*n < 0 || *n > 7)
        {
            printf("Invalid value for 'n'");
            exit(EXIT_FAILURE);
        }
    }
}

bool checkWin(int *deck)
{
    int i;
    for (i = 1; i < 7; i++)
    {
        if (deck[i] % 4 != deck[i - 1] % 4)
            return false;
    }

    return true;
}

int giveCard(thread_arg *targ, unsigned int seed)
{
    int cardId = rand_r(&seed) % 7;
    printf("%d, %d\n", targ->givenCards[(targ->id + 1) % targ->n], targ->deck[cardId]);
    targ->givenCards[(targ->id + 1) % targ->n] = targ->deck[cardId];
    return cardId;
}

void *thread_func(void *arg)
{
    thread_arg *targ = (thread_arg *)arg;
    while (1)
    {
        pthread_barrier_wait(targ->barrier);
        if (checkWin(targ->deck) && targ->id == 0)
        {
            printf("Player %d won!\n", targ->id);
            print_deck(targ->deck, 7);
        }

        pthread_barrier_wait(targ->barrier);

        unsigned int seed = rand();
        int pickedCard = giveCard(targ, seed);

        pthread_barrier_wait(targ->barrier);

        targ->deck[pickedCard] = targ->givenCards[targ->id];

        pthread_barrier_wait(targ->barrier);
    }

    if (sem_post(targ->semaphore) == -1)
        ERR("sem_post");

    free(&targ->deck);
    return NULL;
}

void create_thread(pthread_t *thread, thread_arg *targ, const int *deck, int id, pthread_barrier_t *barrier,
                   sem_t *semaphore, int *givenCards, int n)
{
    srand(time(NULL));
    int *newDeck = (int *)malloc(sizeof(int) * 7);
    for (int j = 0; j < 7; j++)
    {
        newDeck[j] = deck[id * 7 + j];
    }

    targ[id].deck = newDeck;
    targ[id].barrier = barrier;
    targ[id].semaphore = semaphore;
    targ[id].id = id;
    targ[id].seed = rand();
    targ[id].n = n;
    targ[id].givenCards = givenCards;
    if (pthread_create(&thread[id], NULL, thread_func, (void *)&targ[id]) != 0)
        ERR("pthread_create");
}

void do_work(sigset_t oldmask, int n, int *deck)
{
    pthread_t thread[n];
    thread_arg targ[n];
    sem_t semaphore;
    int nOfThreads = 0;
    int threadsAvailable;
    if (sem_init(&semaphore, 0, n) != 0)
        ERR("sem_init");
    pthread_barrier_t barrier;
    int *givenCards = (int *)malloc(sizeof(int) * n);

    pthread_barrier_init(&barrier, NULL, n);
    while (1)
    {
        last_signal = 0;
        while (last_signal != SIGUSR1)
            sigsuspend(&oldmask);
        if (sem_trywait(&semaphore) == -1)
        {
            switch (errno)
            {
                case EAGAIN:
                    fprintf(stderr, "Only %d players can be seated.", n);
                case EINTR:
                    continue;
            }
            ERR("sem_trywait");
        }
        sem_getvalue(&semaphore, &threadsAvailable);
        if (threadsAvailable == n - 1)
        {
            printf("Game ended. Shuffling...\n");
            nOfThreads = 0;
            shuffle(deck, DECK_SIZE);
        }
        create_thread(thread, targ, deck, nOfThreads, &barrier, &semaphore, givenCards, n);
        nOfThreads++;
    }

    pthread_barrier_destroy(&barrier);
    free(givenCards);
}

int main(int argc, char *argv[])
{
    int n;
    ReadArguments(argc, argv, &n);

    int deck[DECK_SIZE];
    for (int i = 0; i < DECK_SIZE; ++i)
        deck[i] = i;
    shuffle(deck, DECK_SIZE);

    set_handler(sigusr_handler, SIGUSR1);
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    do_work(oldmask, n, deck);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    exit(EXIT_SUCCESS);
}
