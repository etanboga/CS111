//
//  main.c
//  lab2_list_proj
//
//  Created by Ege Tanboga on 5/4/18.
//  Copyright © 2018 Tanbooz. All rights reserved.
//

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <assert.h>
#include "SortedList.h"

pthread_mutex_t lock_mutex= PTHREAD_MUTEX_INITIALIZER;
int lock_spin = 0;

long long num_iterations = 1;
int opt_yield = 0;

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

int main(int argc, char **argv) {
    int option;
    int debug = 0;
    int num_threads = 1;
    long long counter = 0;
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
    exit(EXIT_SUCCESS);
}

