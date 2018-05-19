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
#include <mraa/aio.h>

sig_atomic_t volatile run_flag = 1;
const nfds_t NUM_FDS = 1;
const int B = 4275;               // B value of the thermistor
const int R0 = 100000;            // R0 = 100k


void do_when_interrupted(int signal){
    if(signal == SIGINT)
        run_flag = 0;
}

//options

int is_fahrenheit = 1;
int period = 1;
int period_flag = 0;
int log_flag = 0;
int debug = 0;
char* logfile = NULL;
int log_fd;
int can_continue = 1; //checks if our polling should continue, set to false if stop read in commands

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(EXIT_FAILURE);
}

void print_guidelines_and_exit() {
    printf("Usage: ./lab4b --log=filename --scale={C,F} --period=# \n");
    exit(EXIT_FAILURE);
}

ssize_t secure_write(int fd, void* buffer, size_t size) {
    ssize_t bytes_written = write(fd, buffer, size);
    if (bytes_written == -1) {
        print_error_and_exit("Error writing bytes", errno);
    }
    return bytes_written;
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
    while ((option = getopt_long(argc, argv, "p:s:l:d", long_options, NULL)) != -1 ) {
        switch(option) {
            case 'p':
                period_flag = 1;
                period = atoi(optarg);
                break;
            case 's':
                if (strlen(optarg) == 1 && optarg[0] == 'C') {
                    is_fahrenheit = 0;
                } else if (strlen(optarg) == 0 && optarg[0] == 'F') {
                    is_fahrenheit = 1;
                } else if (strlen(optarg) != 0 || optarg[0] != 'F') {
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
    
    //MRAA Initialization
    
    mraa_aio_context temp;
    mraa_gpio_context button;
    temp = mraa_aio_init(1);
    button = mraa_gpio_init(60);
    mraa_gpio_dir(button,MRAA_GPIO_IN);
    signal(SIGINT, do_when_interrupted);
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN|POLLHUP|POLLERR;
    int pollreturn;
    while ((pollreturn = poll(fds, NUM_FDS, 0)) >= 0) {
        if (can_continue) {
            int a = mraa_aio_read(temp);
            //temperature calculation
            float R = 1023.0/a - 1.0;
            R = R0*R;
            float temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15;
            if (is_fahrenheit) {
                temperature = (9.0/5)*temperature + 32;
            }
            char output[70];
            int bytes_sent = sprintf(output, "%.1f\n", temperature);
            if (bytes_sent < 0) {
                print_error_and_exit("Cannot sprintf temperature", errno);
            }
            secure_write(STDOUT_FILENO, output, bytes_sent);
            if (log_flag) {
                secure_write(log_fd, output, bytes_sent);
            }
        }
        if (fds[0].revents & POLLIN) {
            if (debug) {
                printf("Input from keyboard");
            }
        }
        int i;
        int end_loop = 0;
        for (i = 0; i < period; i++) {
            if (mraa_gpio_read(button) || !run_flag) {
                char output[70];
                int bytes_sent = sprintf(output, "SHUTDOWN\n");
                secure_write(STDOUT_FILENO, output, bytes_sent);
                if (log_flag) {
                    secure_write(log_fd, output, bytes_sent);
                }
                end_loop = 1;
                break;
            }
            if (can_continue) {
                usleep(1000000);
            }
        }
        if (end_loop) {
            break;
        }
    }
    mraa_aio_close(temp);
    exit(EXIT_SUCCESS);
}
