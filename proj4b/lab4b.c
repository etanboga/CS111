//NAME: Ege Tanboga
//EMAIL: ege72282@gmail.com
//UID: 304735411

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>

//options

int is_fahrenheit = 1;
int period = 1;
int period_flag = 0;
int log_flag = 0;
int debug = 0;
char* logfile = NULL;
int log_fd;


void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(EXIT_FAILURE);
}

void print_guidelines_and_exit() {
    printf("Usage: ./lab4b --log=filename --scale={C,F} --period=# \n");
    exit(EXIT_SUCCESS);
}



int main(int argc, char * argv[]) {
    static struct option long_options[] = {
        {"period", required_argument, 0, 'p'},
        {"scale", required_argument, 0, 's'},
        {"log", required_argument, 0, 'l'},
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    int option;
    while ((option = getopt_long(argc, argv, "sd", long_options, NULL)) != -1 ) {
        switch(option) {
            case 'p':
                period_flag = 1;
                period = atoi(optarg);
                break;
            case 's':
                if (strlen(optarg) == 1 && optarg[0] == 'C') {
                    is_fahrenheit = 0;
                } else if (strlen(optarg) == 0 && optarg[0] == 'F') {
                    
                } else if (strlen(optarg) == 0 && optarg[0] != 'F') {
                    print_guidelines_and_exit();
                }
                break;
            case 'l':
                log_flag = 1;
                logfile = optarg;
                break;
            case 'd':
                debug = 1;
                break;
            default:
                print_guidelines_and_exit();
        }
    }
    if (debug) {
        printf("Options passed, is_fahrenheit: %d, period: %d, logfile: %s", is_fahrenheit, period, logfile);
    }
    if (period <= 0) {
        print_error_and_exit("Cannot have negative period", EXIT_FAILURE);
    }
    if (log_flag) {
        log_fd = open(logfile, O_NONBLOCK | O_WRONLY | O_APPEND | O_CREAT);
        if (log_fd < 0) {
            print_error_and_exit("log file cannot be opened", errno);
        }
    }
    
}
