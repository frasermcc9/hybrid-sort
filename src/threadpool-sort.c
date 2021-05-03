/*
    The Hybrid Merge Sort to use for Operating Systems Assignment 1 2021
    written by Robert Sheehan

    Modified by: Fraser McCallum
    UPI: frasermcc9

    By submitting a program you are claiming that you and only you have made
    adjustments and additions to this code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/times.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#define SIZE 4
#define MAX 1000
#define SPLIT 16

#define POOL_SIZE 7

typedef struct SimpleJob
{
    void (*function)(void *arg);
    void *arg;
} SimpleJob;

typedef struct Thread
{
    int id;
    pthread_t *thread;
    bool working;
    sem_t *cond;
    SimpleJob *job;
} Thread;

typedef struct ActiveThread
{
    bool available;
    Thread *thread;
} ActiveThread;

struct block
{
    int size;
    int *data;
};

typedef struct ParallelArgs
{
    struct block *block;
    sem_t *caller_sem;
    Thread *threads;
} ParallelArgs;

void *idle_thread(void *args);
ActiveThread *find_idle_thread(ParallelArgs *args);
void mark_thread_busy(Thread *th);
void mark_thread_done(Thread *th);
void launch_thread(Thread *thr);
void execute_job(ParallelArgs *args, void (*fn_ptr)(void *), void *arg_ptr);

void print_data(struct block *block);
void insertion_sort(struct block *block);
void merge(struct block *left, struct block *right);
bool is_sorted(struct block *block);
void produce_random_data(struct block *block);
void *parallel_sort(void *args);

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void execute_job(ParallelArgs *args, void (*fn_ptr)(void *), void *arg_ptr)
{
    pthread_mutex_lock(&lock);
    ActiveThread *optionalThread = find_idle_thread(args);
    if (optionalThread->available)
    {
        mark_thread_busy(optionalThread->thread);
        pthread_mutex_unlock(&lock);
        //puts("THREAD AVAILABLE FOR WORK");
        SimpleJob *new_job = malloc(sizeof(SimpleJob));
        new_job->arg = arg_ptr;
        new_job->function = fn_ptr;
        optionalThread->thread->job = new_job;
        launch_thread(optionalThread->thread);
    }
    else
    {
        //puts("THREAD NOT AVAILABLE FOR WORK");
        pthread_mutex_unlock(&lock);
        fn_ptr(arg_ptr);
    }
}

void launch_thread(Thread *thr)
{

    //puts("THREAD GOT WORK");
    //mark_thread_busy(thr);
    sem_post(thr->cond);
}

void mark_thread_busy(Thread *th)
{
    th->working = true;
}

void mark_thread_done(Thread *th)
{
    th->working = false;
}

// finds an idle thread. Only one thread should call this at a time.
ActiveThread *find_idle_thread(ParallelArgs *args)
{
    ActiveThread *thread = malloc(sizeof(ActiveThread));
    for (int i = 0; i < POOL_SIZE; i++)
    {
        if (!args->threads[i].working)
        {
            thread->available = true;
            thread->thread = &args->threads[i];

            return thread;
        }
    }
    thread->available = false;
    return thread;
}

void *idle_thread(void *args)
{
    Thread *th = (Thread *)args;
    for (;;)
    {
        sem_wait(th->cond);
        //printf("Thread %i has been summoned\n", th->id);
        th->job->function(th->job->arg);
        mark_thread_done(th);
    }
}

int main(int argc, char *argv[])
{
    long size;

    if (argc < 2)
    {
        size = SIZE;
    }
    else
    {
        size = atol(argv[1]);
    }
    struct block block;
    block.size = (int)pow(2, size);
    block.data = (int *)calloc(block.size, sizeof(int));
    if (block.data == NULL)
    {
        perror("Unable to allocate space for data.\n");
        exit(EXIT_FAILURE);
    }

    produce_random_data(&block);
    Thread threads[POOL_SIZE];
    for (int i = 0; i < POOL_SIZE; ++i)
    {
        threads[i].id = i;
        threads[i].working = false;
        threads[i].thread = malloc(sizeof(pthread_t));
        threads[i].cond = malloc(sizeof(sem_t));
        sem_init(threads[i].cond, 0, 0);
        pthread_create(threads[i].thread, NULL, idle_thread, &threads[i]);
    }

    sem_t root_blocker;
    sem_init(&root_blocker, 0, 0);

    ParallelArgs args;
    args.threads = threads;
    args.block = &block;
    args.caller_sem = &root_blocker;

    struct timeval start_wall_time,
        finish_wall_time, wall_time;
    struct tms start_times, finish_times;
    gettimeofday(&start_wall_time, NULL);
    times(&start_times);

    parallel_sort(&args);

    sem_wait(&root_blocker);

    sem_destroy(&root_blocker);

    gettimeofday(&finish_wall_time, NULL);
    times(&finish_times);
    timersub(&finish_wall_time, &start_wall_time, &wall_time);
    printf("start time in clock ticks: %ld\n", start_times.tms_utime);
    printf("finish time in clock ticks: %ld\n", finish_times.tms_utime);
    printf("wall time %ld secs and %ld microseconds\n", wall_time.tv_sec, wall_time.tv_usec);

    if (block.size < 1025)
        print_data(&block);

    printf(is_sorted(&block) ? "sorted\n" : "not sorted\n");
    free(block.data);
    exit(EXIT_SUCCESS);
}

void *parallel_sort(void *voidArgs)
{
    ParallelArgs *args = (ParallelArgs *)voidArgs;
    struct block *block = args->block;

    if (block->size > SPLIT)
    {
        struct block left_block;
        struct block right_block;
        left_block.size = block->size / 2;
        left_block.data = block->data;
        right_block.size = block->size - left_block.size; // left_block.size + (block->size % 2);
        right_block.data = block->data + left_block.size;

        sem_t local_sem;
        sem_t redundant_sem;
        sem_init(&local_sem, 0, 0);
        sem_init(&redundant_sem, 0, 0);

        ParallelArgs left_args;
        left_args.threads = args->threads;
        left_args.block = &left_block;
        left_args.caller_sem = &redundant_sem;

        ParallelArgs right_args;
        right_args.threads = args->threads;
        right_args.block = &right_block;
        right_args.caller_sem = &local_sem;

        execute_job(&right_args, (void *)parallel_sort, &right_args);
        parallel_sort(&left_args);

        sem_wait(&local_sem);

        sem_destroy(&local_sem);
        sem_destroy(&redundant_sem);

        merge(&left_block, &right_block);
    }
    else
    {
        insertion_sort(block);
    }
    sem_post(args->caller_sem);
}

void print_data(struct block *block)
{
    for (int i = 0; i < block->size; ++i)
        printf("%d ", block->data[i]);
    printf("\n");
}

/* The insertion sort for smaller halves. */
void insertion_sort(struct block *block)
{
    for (int i = 1; i < block->size; ++i)
    {
        for (int j = i; j > 0; --j)
        {
            if (block->data[j - 1] > block->data[j])
            {
                int temp;
                temp = block->data[j - 1];
                block->data[j - 1] = block->data[j];
                block->data[j] = temp;
            }
        }
    }
}

/* Combine the two halves back together. */
void merge(struct block *left, struct block *right)
{
    int *combined = calloc(left->size + right->size, sizeof(int));
    if (combined == NULL)
    {
        perror("Allocating space for merge.\n");
        exit(EXIT_FAILURE);
    }
    int dest = 0, l = 0, r = 0;
    while (l < left->size && r < right->size)
    {
        if (left->data[l] < right->data[r])
            combined[dest++] = left->data[l++];
        else
            combined[dest++] = right->data[r++];
    }
    while (l < left->size)
        combined[dest++] = left->data[l++];
    while (r < right->size)
        combined[dest++] = right->data[r++];
    memmove(left->data, combined, (left->size + right->size) * sizeof(int));
    free(combined);
}

/* Merge sort the data. */
void merge_sort(struct block *block)
{
    if (block->size > SPLIT)
    {
        struct block left_block;
        struct block right_block;
        left_block.size = block->size / 2;
        left_block.data = block->data;
        right_block.size = block->size - left_block.size; // left_block.size + (block->size % 2);
        right_block.data = block->data + left_block.size;
        merge_sort(&left_block);
        merge_sort(&right_block);
        merge(&left_block, &right_block);
    }
    else
    {
        insertion_sort(block);
    }
}

/* Check to see if the data is sorted. */
bool is_sorted(struct block *block)
{
    bool sorted = true;
    for (int i = 0; i < block->size - 1; i++)
    {
        if (block->data[i] > block->data[i + 1])
            sorted = false;
    }
    return sorted;
}

/* Fill the array with random data. */
void produce_random_data(struct block *block)
{
    srand(1); // the same random data seed every time
    for (int i = 0; i < block->size; i++)
    {
        block->data[i] = rand() % MAX;
    }
}