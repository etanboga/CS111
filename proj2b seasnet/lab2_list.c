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
#include "SortedList.h"
#include <signal.h>

pthread_mutex_t lock_mutex= PTHREAD_MUTEX_INITIALIZER;
int lock_spin = 0;

long long num_iterations = 1;
int opt_yield = 0;

void* (*current_function)(void *);

SortedList_t* list;
SortedListElement_t* elements;

//wrappers for mutex_lock

void Pthread_mutex_lock(pthread_mutex_t *lock) {
    int ret = pthread_mutex_lock(lock);
    assert(ret == 0);
}

void Pthread_mutex_unlock(pthread_mutex_t *lock) {
    int ret = pthread_mutex_unlock(lock);
    assert(ret == 0);
}
void* manipulate_list(void *arg) {
    SortedListElement_t *start = arg;
    int i;
    for (i = 0; i < num_iterations; i++) {
        SortedList_insert(list, start + i);
    }
    int length = SortedList_length(list);
    if (length == -1) {
        fprintf(stderr, "List corrupted after insertion");
        exit(2);
    }
    for (i = 0; i < num_iterations; i++) {
        SortedListElement_t* search_result = SortedList_lookup(list, start[i].key);
        if (search_result == NULL) {
            fprintf(stderr, "List corrupted when looking for inserted element");
            exit(2);
        }
        int delete_ret = SortedList_delete(search_result);
        if (delete_ret == 1) {
            fprintf(stderr, "List corrupted while deletion");
            exit(2);
        }
    }
    return NULL;
}

void* manipulate_list_mutex(void *arg) {
    SortedListElement_t *start = arg;
    int i;
    for (i = 0; i < num_iterations; i++) {
        Pthread_mutex_lock(&lock_mutex);
        SortedList_insert(list, start + i);
        Pthread_mutex_unlock(&lock_mutex);
    }
    Pthread_mutex_lock(&lock_mutex);
    int length = SortedList_length(list);
    Pthread_mutex_unlock(&lock_mutex);
    if (length == -1) {
        fprintf(stderr, "List corrupted after insertion");
        exit(2);
    }
    for (i = 0; i < num_iterations; i++) {
        Pthread_mutex_lock(&lock_mutex);
        SortedListElement_t* search_result = SortedList_lookup(list, start[i].key);
        if (search_result == NULL) {
            fprintf(stderr, "List corrupted when looking for inserted element");
            exit(2);
        }
        int delete_ret = SortedList_delete(search_result);
        Pthread_mutex_unlock(&lock_mutex);
        if (delete_ret == 1) {
            fprintf(stderr, "List corrupted while deletion");
            exit(2);
        }
    }
    return NULL;
}

void* manipulate_list_spin(void *arg) {
    SortedListElement_t *start = arg;
    int i;
    for (i = 0; i < num_iterations; i++) {
        while (__sync_lock_test_and_set(&lock_spin, 1) == 1) {
            ; //spin
        }
        SortedList_insert(list, start + i);
        __sync_lock_release(&lock_spin);
    }
    while (__sync_lock_test_and_set(&lock_spin, 1) == 1) {
        ; //spin
    }
    int length = SortedList_length(list);
    __sync_lock_release(&lock_spin);
    if (length == -1) {
        fprintf(stderr, "List corrupted after insertion");
        exit(2);
    }
    for (i = 0; i < num_iterations; i++) {
        while (__sync_lock_test_and_set(&lock_spin, 1) == 1) {
            ; //spin
        }
        SortedListElement_t* search_result = SortedList_lookup(list, start[i].key);
        if (search_result == NULL) {
            fprintf(stderr, "List corrupted when looking for inserted element");
            exit(2);
        }
        int delete_ret = SortedList_delete(search_result);
        __sync_lock_release(&lock_spin);
        if (delete_ret == 1) {
            fprintf(stderr, "List corrupted while deletion");
            exit(2);
        }
    }
    return NULL;
}


typedef enum locks {
    none, mutex, spin
} lock_type;

lock_type current_lock = none; //use no lock by default

void print_guidelines_and_exit() {
    printf("Usage: ./lab2_list --threads=num_threads --iterations=num_iterations --yield=[idl] --sync=[ms] \n");
    exit(EXIT_FAILURE);
}

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(EXIT_FAILURE);
}

void signal_handler(int signal) {
    if (signal == SIGSEGV) {
        fprintf(stderr, "Caught a segmentation fault, error: %d, error message: %s \n",signal, strerror(signal));
        exit(2);
    }
}

void generate_elements(long long num_elements) {
    srand(time(NULL));
    int i;
    for (i = 0; i < num_elements; i++) {
        int length = rand() % 20 + 1; //random bounds
        int letter = rand() % 26; //goes through each character in alphabet
        char* key = malloc((length + 1 ) * sizeof(char));
        int j;
        for (j = 0; j < length; j++) {
            key[j] = 'a' + letter;
            letter = rand() % 26;
        }
        key[length] = '\0';
        elements[i].key = key;
    }
}


int main(int argc, char **argv) {
    int option;
    int debug = 0;
    int num_threads = 1;
    pthread_t* threads;
    static struct option long_options[] = {
        {"threads", required_argument, 0, 't'},
        {"iterations", required_argument, 0, 'i'},
        {"yield", required_argument, 0, 'y'},
        {"sync", required_argument, 0, 's'},
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    int insert_yield = 0;
    int delete_yield = 0;
    int lookup_yield = 0;
    while ((option = getopt_long(argc, argv, "t:i:y:s:d", long_options, NULL)) != -1) {
        unsigned long i;
        switch (option) {
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'i':
                num_iterations = atoi(optarg);
                break;
            case 'y':
            {
                
                for (i=0; i< strlen(optarg); i++) {
                    if (optarg[i] == 'i') {
                        opt_yield |= INSERT_YIELD;
                        insert_yield = 1;
                    } else if (optarg[i] == 'd') {
                        opt_yield |= DELETE_YIELD;
                        delete_yield = 1;
                    } else if (optarg[i] == 'l') {
                        opt_yield |= LOOKUP_YIELD;
                        lookup_yield = 1;
                    } else {
                        print_guidelines_and_exit();
                    }
                }
            }
                break;
            case 's':
            {
                if (strlen(optarg) == 1 && (optarg[0] == 'm')) {
                    current_lock = mutex;
                } else if ((strlen(optarg) == 1) && (optarg[0] == 's')) {
                    current_lock = spin;
                } else {
                    print_guidelines_and_exit();
                }
            }
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
        printf("Passed in arguments are: num_threads: %d, num_iterations: %lld, insert_yield: %d, delete_yield: %d, lookup_yield: %d, lock type: %d", num_threads, num_iterations, insert_yield, delete_yield, lookup_yield, current_lock);
    }
    if (signal(SIGSEGV, signal_handler) == SIG_ERR) {
        print_error_and_exit("Error setting up signal handler", errno);
    }
    
    char* lock_text = "none";
    switch (current_lock) {
        case none:
            current_function = manipulate_list;
            lock_text = "none";
            break;
        case mutex:
            current_function = manipulate_list_mutex;
            lock_text = "m";
            break;
        case spin:
            current_function = manipulate_list_spin;
            lock_text = "s";
            break;
        default:
            if (debug) {
                printf("Entered impossible condition in current_lock switch, must be a problem");
            }
            break;
    }
    
    list  = malloc(sizeof(SortedList_t));
    threads = malloc(sizeof(pthread_t) * num_threads);
    list->prev = list;
    list->next = list;
    list->key= NULL;
    long long num_elements = num_threads * num_iterations;
    elements = malloc(sizeof(SortedListElement_t) * num_elements);
    generate_elements(num_elements);
    if (debug) {
        printf("First element: %s", elements[0].key);
    }
    int i;
    struct timespec start_time, end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        print_error_and_exit("Couldn't get clock time for start time", errno);
    }
    for (i = 0; i < num_threads; i++) {
            if (pthread_create(threads + i, NULL, current_function, elements + i * num_iterations) != 0) {
                print_error_and_exit("Error: creating thread", errno);
        } else if (debug) {
                printf("Created thread: %d", i);
        }
    }
    for (i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            print_error_and_exit("Error: joining threads", errno);
        } else if (debug) {
            printf("Joining thread: %d", i);
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        print_error_and_exit("Couldn't get clock time for end time", errno);
    }
    if (SortedList_length(list) != 0) {
        fprintf(stderr, "Error: Sorted list doesn't have length 0 at the end of computation");
        exit(2);
    }
    free(threads);
    free(elements);
    free(list);
    long long nanoseconds = (end_time.tv_sec - start_time.tv_sec) * 1000000000L + (end_time.tv_nsec - start_time.tv_nsec);
    int num_lists = 1;
    long long operations_performed = num_threads * num_iterations * 3;
    long long avg_per_op = nanoseconds / operations_performed;
    char *yield_description = "test"; //should never be test
    if (opt_yield == 0) {
        yield_description = "none";
    } else if (delete_yield == 1 && insert_yield == 1 && lookup_yield == 1) {
        yield_description = "idl";
    } else if (insert_yield == 1 && lookup_yield == 1 && delete_yield == 0) {
        yield_description= "il";
    } else if (insert_yield == 1 && delete_yield == 1 && lookup_yield == 0) {
        yield_description = "id";
    } else if (lookup_yield == 1 && delete_yield == 1 && insert_yield == 0) {
        yield_description = "dl";
    } else if (insert_yield == 1 && delete_yield == 0 && lookup_yield == 0) {
        yield_description = "i";
    } else if (insert_yield == 0 && delete_yield == 0 && lookup_yield == 1) {
        yield_description = "l";
    } else {
        yield_description = "d";
    }
    printf("list-%s-%s,%d,%lld,%d,%lld,%lld,%lld \n", yield_description, lock_text, num_threads, num_iterations, num_lists, operations_performed, nanoseconds, avg_per_op);
    exit(EXIT_SUCCESS);
}

