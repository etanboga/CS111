//NAME: Ege Tanboga
//EMAIL: ege72282@gmail.com
//ID: 304735411

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <assert.h>

//a function pointer to see which function we should use

void (*current_function) (long long*, long long);

long long num_iterations = 1;
int opt_yield = 0;

pthread_mutex_t lock_mutex = PTHREAD_MUTEX_INITIALIZER;
int lock_spin = 0;

//wrappers for mutex_lock

void Pthread_mutex_lock(pthread_mutex_t *lock) {
    int ret = pthread_mutex_lock(lock);
    assert(ret == 0);
}

void Pthread_mutex_unlock(pthread_mutex_t *lock) {
    int ret = pthread_mutex_unlock(lock);
    assert(ret == 0);
}

void add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    if (opt_yield)
        sched_yield();
    *pointer = sum;
}

void add_mutex(long long *pointer, long long value) {
    Pthread_mutex_lock(&lock_mutex);
    add(pointer, value);
    Pthread_mutex_unlock(&lock_mutex);
}

void add_spin(long long *pointer, long long value) {
    while(__sync_lock_test_and_set(&lock_spin, 1) == 1) {
        ;
    }
    add(pointer, value);
    __sync_lock_release(&lock_spin);
}

void add_sync(long long *pointer, long long value) {
    long long expected;
    long long sum;
    do{
        expected = *pointer;
        sum = *pointer + value;
        if(opt_yield)
            sched_yield();
    } while(expected != __sync_val_compare_and_swap(pointer, expected, sum));
}

void* thread_compute(void* counter) {
    int i;
    for (i = 0; i< num_iterations; i++) {
        current_function((long long *) counter, -1);
    }
    for (i = 0; i< num_iterations; i++) {
        current_function((long long *) counter, 1);
    }
    return NULL;
}


typedef enum locks {
    none, mutex, spin, compare_and_swap
} lock_type;

lock_type current_lock = none; //use no lock by default

void print_guidelines_and_exit() {
    printf("Usage: ./lab2_add --threads=num_threads --iterations=num_iterations --yield --sync={m,s,c} \n");
    exit(EXIT_FAILURE);
}

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    int option;
    int debug = 0;
    pthread_t *threads;
    int num_threads = 1;
    long long counter = 0;
    static struct option long_options[] = {
        {"threads", required_argument, 0, 't'},
        {"iterations", required_argument, 0, 'i'},
        {"sync", required_argument, 0, 's'},
        {"yield", no_argument, 0, 'y'},
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    while ((option = getopt_long(argc, argv, "t:i:syd", long_options, NULL)) != -1) {
        switch (option) {
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'i':
                num_iterations = strtoll(optarg, NULL, 10);
                break;
            case 's':
            {
                if (strlen(optarg) == 1 && (optarg[0] == 'm')) {
                    current_lock = mutex;
                } else if ((strlen(optarg) == 1) && (optarg[0] == 's')) {
                    current_lock = spin;
                } else if ((strlen(optarg) == 1) && (optarg[0] == 'c')) {
                    current_lock = compare_and_swap;
                }
            }
                break;
            case 'y':
                opt_yield = 1;
                break;
            case 'd':
                debug = 1;
                break;
            default:
                print_guidelines_and_exit();
        
        }
    }
    
    if (debug) {
        //check if arguments passed properly
        printf("Passed in arguments are: num_threads: %d, num_iterations: %lld, opt_yield: %d, lock type: %d", num_threads, num_iterations, opt_yield, current_lock);
    }
    threads = malloc(num_threads*sizeof(pthread_t));
    char* lock_text = "none";
    switch (current_lock) {
        case none:
            current_function = add;
            lock_text = "none";
            break;
        case mutex:
            current_function = add_mutex;
            lock_text = "m";
            break;
        case spin:
            current_function = add_spin;
            lock_text = "s";
            break;
        case compare_and_swap:
            current_function = add_sync;
            lock_text = "c";
            break;
        default:
            if (debug) {
                printf("Entered impossible condition in current_lock switch, must be a problem");
            }
            break;
    }
    struct timespec start_time, end_time;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time) != 0) {
        print_error_and_exit("Couldn't get clock time for start time", errno);
    }
    int i;
    for (i = 0; i < num_threads; i++) {
        if (pthread_create(threads + i, NULL, thread_compute, &counter) != 0) {
            print_error_and_exit("Error creating threads", errno);
        }
    }
    for (i = 0; i < num_threads; i++) {
        if (pthread_join(*(threads + i), NULL) != 0) {
            print_error_and_exit("Error joining threads", errno);
        }
    }
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time) != 0) {
        print_error_and_exit("Couldn't get clock time for end time", errno);
    }
    long long nanoseconds = (end_time.tv_sec - start_time.tv_sec) * 1000000000L + (end_time.tv_nsec - start_time.tv_nsec);
    long long total_runtime = num_threads * num_iterations * 2;
    printf("add%s-%s,%d,%lld,%lld,%lld,%lld,%lld\n",
           opt_yield == 1 ? "-yield" : "", lock_text, num_threads, num_iterations, total_runtime, nanoseconds, nanoseconds / total_runtime, counter);
    free(threads);
    exit(EXIT_SUCCESS);
}

