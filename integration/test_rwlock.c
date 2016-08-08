/**
 ** test utility for read-write lock using sysv semaphores
 **
 **
 **/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include "rwlock.h"

#define THREADS                             10

#define N_SEMS                              128

#define MAX_DATA_LN                         4096

#define rotate64(v, n)                      (((v) << (n)) | ((v) >> (64 - (n))))

struct bucket
{
    uint64_t                                checksum;
    size_t                                  ln;
    uint8_t                                 data[MAX_DATA_LN];

};

struct readlock                            *locks;

struct bucket                               bucket;


static void initialise_locks()
{
    int                                     i;

    locks = malloc(N_SEMS * sizeof(struct readlock));

    for (i = 0; i < N_SEMS; i++)
    {
        locks[i] = readlock_init;
    }

}

void update_bucket(struct bucket *bucket)
{
    uint64_t                                checksum = 0x43f42a71e03;
    int                                     i;

    bucket->ln = random() % MAX_DATA_LN;

    for (i = 0; i < bucket->ln; i++)
    {
        bucket->data[i] = random();

        checksum = rotate64(checksum, 3) ^ bucket->data[i];
    }
    
    bucket->checksum = checksum;

}

int verify_bucket(struct bucket *bucket)
{
    uint64_t                                checksum = 0x43f42a71e03;
    int                                     i;

    for (i = 0; i < bucket->ln; i++)
    {
        checksum = rotate64(checksum, 3) ^ bucket->data[i];
    }
    
    return bucket->checksum == checksum;

}

void *multi_lock_thread(void *data)
{
    pid_t                                   pid = getpid();
    void                                   *self = pthread_self();

    int                                     i;
    int                                     updates = 0, busy = 0;

    for (i = 0; i < 10000000; i++)
    {
        int                                 l = 0;//rand() % N_SEMS;

        if (i&1)
        {
            if (read_lock(locks + l, pid))
            {
                if (verify_bucket(&bucket) == 0)
                {
                    printf("******** bucket not stable, lock counter -> %d\n", locks[l].readers);
                    pthread_exit(0);
                }
                read_release(locks + l, pid);
            }
            else
            {
                printf("%d:%p failed lock %d readlock\n", pid, self, (int)l);
            }
        }
        else
        {
            if (read_lock(locks + l, pid))
            {
                if (read_try_unique(locks + l, 10))
                {
                    update_bucket(&bucket);
                    read_release_unique(locks + l);

                    updates++;
                }
                else
                {
                    busy++;
                }
                read_release(locks + l, pid);
            }
            else
            {
                printf("%d:%p failed lock %d writelock\n", pid, self, (int)l);
            }
        }

        if (i % 100000 == 0)
        {
            printf("%d:%p iteration %d, updates %d out of %d\n", pid, self, i, updates, busy);
        }
    }

    return data;

}

void *single_lock_thread(void *data)
{
    pid_t                                   pid = getpid();
    void                                   *self = pthread_self();

    int                                     i;
    int                                     updates = 0, busy = 0;

    for (i = 0; i < 10000000; i++)
    {
        int                                 l = 0;

        if (random()&1)
        {
            if (read_lock(locks + l, pid))
            {
                if (verify_bucket(&bucket) == 0)
                {
                    printf("******** bucket not stable\n");
                }
                read_release(locks + l, pid);
            }
            else
            {
                printf("%d:%p failed lock %d readlock\n", pid, self, (int)l);
            }
        }
        else
        {
            if (read_lock(locks + l, pid))
            {
                if (read_try_unique(locks + l, 3))
                {
                    update_bucket(&bucket);
                    read_release_unique(locks + l);

                    updates++;
                } 
                else
                {
                    busy++;
                }

                read_release(locks + l, pid);
            }
            else
            {
                printf("%d:%p failed lock %d writelock\n", pid, self, (int)l);
            }
        }

        if (i % 100000 == 0)
        {
            printf("%d:%p iteration %d, updates %d out of %d\n", pid, self, i, updates, busy);
        }
    }

    return data;

}


int main(int argc, char *argv[])
{
    pthread_t                               threads[THREADS];
    long                                    args[THREADS];
    
    int                                     i;
    long                                    t0;
    double                                  dt;

    initialise_locks();

    update_bucket(&bucket);

    t0 = clock();
    
    for (i = 0; i < THREADS; i++)
    {
        args [i] = 0;

        if (pthread_create(threads + i, NULL, multi_lock_thread, args + i))
            perror("create thread");
    }

    for (i = 0; i < THREADS; i++)
    {
        void                               *arg = 0;
        
        if (pthread_join(threads[i], &arg))
            perror("join thread");
    }
    
    dt = ((double) (clock() - t0)) / CLOCKS_PER_SEC;
    printf("finished after %lf secs\n", dt);
    
    exit(0);

}
