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



void add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    *pointer = sum;
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
    int num_iterations = 1;
    int opt_yield = 0;
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
                num_iterations = atoi(optarg);
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
        printf("Passed in arguments are: num_threads: %d, num_iterations: %d, opt_yield: %d, lock type: %d", num_threads, num_iterations, opt_yield, current_lock);
    }
}

