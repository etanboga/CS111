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

#define MULTIPLIER (37)

unsigned long
hash(const char *s)
{
    unsigned long h;
    unsigned const char *us;
    
    /* cast s to unsigned const char * */
    /* this ensures that elements of s will be treated as having values >= 0 */
    us = (unsigned const char *) s;
    
    h = 0;
    while(*us != '\0') {
        h = h * MULTIPLIER + *us;
        us++;
    }
    
    return h;
}

int num_lists = 1;

typedef enum locks {
    none, mutex, spin
} lock_type;

typedef struct sub_list {
    SortedList_t* sublist;
    pthread_mutex_t lock_mutex;
    int lock_spin;
} sub_list;

typedef struct {
    sub_list* sublists;
    int num_lists;
} main_list;
main_list parent_list;

lock_type current_lock = none; //use no lock by default

long long num_iterations = 1;
int opt_yield = 0;


SortedListElement_t* elements;

void print_guidelines_and_exit() {
    printf("Usage: ./lab2_list --threads=num_threads --iterations=num_iterations --lists=num_lists --yield=[idl] --sync=[ms] \n");
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

//wrappers for mutex_lock

void Pthread_mutex_lock(pthread_mutex_t *lock) {
    int ret = pthread_mutex_lock(lock);
    assert(ret == 0);
}

void Pthread_mutex_unlock(pthread_mutex_t *lock) {
    int ret = pthread_mutex_unlock(lock);
    assert(ret == 0);
}

void lock_spin_function(unsigned long hash_id, struct timespec* total_time) {
    struct timespec start_time, end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        print_error_and_exit("Couldn't get clock start time in spin function", errno);
    }
    while (__sync_lock_test_and_set(&parent_list.sublists[hash_id].lock_spin, 1) == 1) {
        ; //spin
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        print_error_and_exit("Couldn't get clock end time in spin function", errno);
    }
    total_time->tv_sec += end_time.tv_sec - start_time.tv_sec;
    total_time->tv_nsec += end_time.tv_nsec - start_time.tv_nsec;
}

void lock_mutex_function(unsigned long hash_id, struct timespec* total_time) {
    struct timespec start_time, end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        print_error_and_exit("Couldn't get clock start time in mutex function", errno);
    }
    Pthread_mutex_lock(&parent_list.sublists[hash_id].lock_mutex);
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        print_error_and_exit("Couldn't get clock end time in mutex function", errno);
    }
    total_time->tv_sec += end_time.tv_sec - start_time.tv_sec;
    total_time->tv_nsec += end_time.tv_nsec - start_time.tv_nsec;
}
void lock_function(unsigned long hash_id, struct timespec* total_time) {
    if (current_lock == spin) {
        lock_spin_function(hash_id, total_time);
    } else if (current_lock == mutex) {
        lock_mutex_function(hash_id, total_time);
    }
}

void unlock(unsigned long hash_id) {
    if (current_lock == spin) {
        __sync_lock_release(&parent_list.sublists[hash_id].lock_spin);
    } else if (current_lock == mutex) {
        Pthread_mutex_unlock(&parent_list.sublists[hash_id].lock_mutex);
    }
}


void* manipulate_list(void *arg, struct timespec *total_time) {
    SortedListElement_t *start = arg;
    int i;
    for (i = 0; i < num_iterations; i++) {
        unsigned long hash_id = hash((start + i)->key) % parent_list.num_lists;
        if (current_lock != none) {
            lock_function(hash_id, total_time);
        }
        SortedList_insert(parent_list.sublists[hash_id].sublist, start + i);
        if (current_lock != none) {
            unlock(hash_id);
        }
    }
    int length = 0;
    for (i = 0; i < parent_list.num_lists; i++) {
        if (current_lock != none) {
            lock_function(i, total_time);
        }
        int sublist_length = SortedList_length(parent_list.sublists[i].sublist);
        if (sublist_length == -1) {
            fprintf(stderr, "List corrupted when measuring length");
            exit(2);
        }
        length += sublist_length;
        if (current_lock != none) {
            unlock(i);
        }
    }
    if (length == -1) {
        fprintf(stderr, "List corrupted after length measurement");
        exit(2);
    }
    for (i = 0; i < num_iterations; i++) {
        unsigned long hash_id = hash((start + i)-> key) % parent_list.num_lists;
        if (current_lock != none) {
            lock_function(hash_id, total_time);
        }
        SortedListElement_t* search_result = SortedList_lookup(parent_list.sublists[hash_id].sublist, start[i].key);
        if (search_result == NULL) {
            fprintf(stderr, "List corrupted when looking for inserted element");
            exit(2);
        }
        int delete_ret = SortedList_delete(search_result);
        if (delete_ret == 1) {
            fprintf(stderr, "List corrupted while deletion");
            exit(2);
        }
        if (current_lock != none) {
            unlock(hash_id);
        }
    }
    return total_time;
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

void* current_function(void *arg) {
    struct timespec *total_time = NULL;
    if (current_lock != none) {
        total_time = malloc(sizeof(*total_time));
        if (total_time == NULL) {
            print_error_and_exit("Error allocating memory for counter", errno);
        }
        memset(total_time, 0, sizeof(*total_time));
    }
    return manipulate_list(arg, total_time);
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
        {"lists", required_argument, 0, 'l'},
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
            case 'l':
                num_lists = atoi(optarg);
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
        printf("Passed in arguments are: num_threads: %d, num_iterations: %lld, insert_yield: %d, delete_yield: %d, lookup_yield: %d, lock type: %d, num_lists: %d", num_threads, num_iterations, insert_yield, delete_yield, lookup_yield, current_lock, num_lists);
    }
    if (signal(SIGSEGV, signal_handler) == SIG_ERR) {
        print_error_and_exit("Error setting up signal handler", errno);
    }
    
    char* lock_text = "none";
    switch (current_lock) {
        case none:
            lock_text = "none";
            break;
        case mutex:
            lock_text = "m";
            break;
        case spin:
            lock_text = "s";
            break;
        default:
            if (debug) {
                printf("Entered impossible condition in current_lock switch, must be a problem");
            }
            break;
    }
    
    parent_list.num_lists = num_lists;
    parent_list.sublists = malloc(num_lists * sizeof(*parent_list.sublists));
    if (parent_list.sublists == NULL) {
        print_error_and_exit("Couldn't allocate memory for sublists", errno);
    }
    memset(parent_list.sublists, 0, num_lists * sizeof(*parent_list.sublists));
    
    //initialization for sublists
    
    int l;
    for (l = 0; l < num_lists; l++) {
        sub_list * lsub = parent_list.sublists + l;
        lsub->sublist = malloc(sizeof(SortedList_t));
        if (lsub->sublist == NULL) {
            print_error_and_exit("Coudln't allocate memory for sublist", errno);
        }
        lsub->sublist->key = NULL;
        lsub->sublist->next = lsub->sublist->prev = lsub->sublist;
        pthread_mutex_init(&lsub->lock_mutex, NULL);
        lsub->lock_spin = 0;
    }
    threads = malloc(sizeof(pthread_t) * num_threads);
    long long num_elements = num_threads * num_iterations;
    elements = malloc(sizeof(SortedListElement_t) * num_elements);
    generate_elements(num_elements);
    if (debug) {
        printf("First element: %s", elements[0].key);
    }
    int i;
    struct timespec start_time, end_time;
    struct timespec time_locked;
    memset(&time_locked, 0, sizeof(time_locked));
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
        void *total_time;
        if (pthread_join(threads[i], &total_time) != 0) {
            print_error_and_exit("Error: joining threads", errno);
        }
        if (total_time != NULL) {
            time_locked.tv_sec += ((struct timespec *) total_time)->tv_sec;
            time_locked.tv_nsec += ((struct timespec *) total_time)->tv_nsec;
            if (debug) {
                printf("Time in nanoseconds: %ld \n", time_locked.tv_nsec + 1000000000L * time_locked.tv_sec);
            }
        } else if (debug) {
            printf("total time is null");
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        print_error_and_exit("Couldn't get clock time for end time", errno);
    }
    free(threads);
    free(elements);
    long long nanoseconds = (end_time.tv_sec - start_time.tv_sec) * 1000000000L + (end_time.tv_nsec - start_time.tv_nsec);
    long long lock_nanoseconds = time_locked.tv_nsec + 1000000000L * time_locked.tv_sec;
    if (debug) {
        printf("Lock nanoseconds: %lld", lock_nanoseconds);
    }
    long long lock_operations_performed = num_threads * 3 * (num_iterations + 1);
    long long operations_performed = num_threads * num_iterations * 3;
    long long avg_per_op = nanoseconds / operations_performed;
    long long avg_per_lock = lock_nanoseconds / lock_operations_performed;
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
    printf("list-%s-%s,%d,%lld,%d,%lld,%lld,%lld,%lld \n", yield_description, lock_text, num_threads, num_iterations, num_lists, operations_performed, nanoseconds, avg_per_op, avg_per_lock);
    exit(EXIT_SUCCESS);
}

