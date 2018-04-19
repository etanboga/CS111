//Name: Ege Tanboga
//email: ege72282@gmail.com
//UID: 304735411

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 256

struct termios default_terminal_state;
struct termios program_terminal_state;

int debug = 0;
int shell = 0;
int log_flag = 0;
int port_number_flag = 0;
int port_number;
char* logfile;

struct pollfd poll_file_d[2];

//define constants for exit state

const int FAIL_EXIT_CODE = 1;
const int SUCCESS_EXIT_CODE = 0;
const nfds_t NUM_FDS = 2;

//buffer with recommended size in the spec

static char buff[BUFFER_SIZE];

//MARK: - print functions

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(FAIL_EXIT_CODE);
}

void print_guidelines_and_exit() {
    printf("Usage: ./lab1b-client --port=portnumber --log=filename --compress ----debug \n");
    exit(FAIL_EXIT_CODE);
}

//MARK: - terminal functions

void save_terminal_state() {
    int term = tcgetattr(STDIN_FILENO, &default_terminal_state);
    if (term == -1) {
        print_error_and_exit("Error saving terminal state", errno);
    } else if (debug) {
        printf("Saved terminal state successfully\n");
    }
}

void restore_terminal_state() {
    int restoreterm = tcsetattr(STDIN_FILENO, TCSANOW, &default_terminal_state);
    if (restoreterm == -1) {
        print_error_and_exit("Couldn't restore terminal state", errno);
    }
    if (debug) {
        printf("restored terminal state properly for default behaviour");
    }
}

void set_program_terminal_state() {
    int term = tcgetattr(STDIN_FILENO, &program_terminal_state);
    if (term == -1) {
        print_error_and_exit("Error saving terminal state before setting it for the program", errno);
    }
    program_terminal_state.c_iflag = ISTRIP;
    program_terminal_state.c_oflag = 0;
    program_terminal_state.c_lflag = 0;
    int setterm = tcsetattr(STDIN_FILENO, TCSANOW, &program_terminal_state);
    if (setterm == -1) {
        print_error_and_exit("Couldn't set program terminal state", errno);
    } else if (debug) {
        printf("Set terminal state\n");
    }
}


//read and write functions

ssize_t secure_read(int fd, void* buffer, size_t size) {
    ssize_t bytes_read = read(fd, buffer, size);
    if (bytes_read == -1) {
        print_error_and_exit("Error reading from keyboard", errno);
    }
    return bytes_read;
}

ssize_t secure_write(int fd, void* buffer, size_t size) {
    ssize_t bytes_written = write(fd, buffer, size);
    if (bytes_written == -1) {
        print_error_and_exit("Error writing bytes", errno);
    }
    return bytes_written;
}

void write_many(int fd, char* buffer, size_t size, int is_to_shell) {
    size_t i;
    for (i = 0; i < size; i++) {
        char* current_char_ptr  = (buffer+i);
        switch(*current_char_ptr) {
            case 0x0D: //for \r -> Carriage return
            case 0x0A:
                if (is_to_shell) { //if writing to shell, write as newline
                    char newline[1];
                    newline[0] = '\n';
                    secure_write(fd,newline, 1);
                }
                else { //for \n -> newline, if writing to stdout, nothing if
                    //                    if (debug) {    //writing to shell
                    //                        printf("Entered option for EOF, when not writing to shell \n");
                    //                    }
                    char eofchars[2] = {'\r', '\n'};
                    secure_write(fd, eofchars, 2);
                }
                break;
            default:
                secure_write(fd, current_char_ptr, 1);
        }
    }
}



//int process_poll() {
//    int should_continue_loop = 1;
//    int pollresult = poll(poll_file_d, NUM_FDS, 0);
//    if (pollresult == -1) {
//        print_error_and_exit("Error setting up poll", errno);
//    }
//    //if none of the file descriptors are ready, return for next iteration
//    if (pollresult == 0) {
//        return should_continue_loop;
//    }
//    if (poll_file_d[0].revents & POLLIN) {
//        //we can read from standard input
//        char buffer_stdin[BUFFER_SIZE];
//        ssize_t bytes_read = secure_read(poll_file_d[0].fd, buffer_stdin, BUFFER_SIZE);
//        write_many(STDOUT_FILENO, buffer_stdin, bytes_read, 0); //write out
//        write_many(to_shell[1], buffer_stdin, bytes_read, 1); //send to shell
//    }
//    if (poll_file_d[1].revents & POLLIN) {
//        //we can read from the shell
//        char buffer_shell[BUFFER_SIZE];
//        ssize_t bytes_read = secure_read(poll_file_d[1].fd, buffer_shell, BUFFER_SIZE);
//        write_many(STDOUT_FILENO, buffer_shell, bytes_read, 0);
//    }
//    if (poll_file_d[1].revents & (POLLERR | POLLHUP)) {
//        //shell shut down
//        int returnclose = close(to_shell[1]); //stop writing to shell
//        if (returnclose == -1) {
//            print_error_and_exit("Couldn't close writing to shell in POLLERR", errno);
//        }
//        should_continue_loop = -1;
//    }
//    return should_continue_loop;
//}


//void pre_parent_setup() {
//    int returnclose_parent = close(to_terminal[1]); //don't need to write to terminal
//    if (returnclose_parent == -1) {
//        print_error_and_exit("Couldn't close pipe to write to terminal in parent", errno);
//    }
//    returnclose_parent = close(to_shell[0]); //don't need to read from terminal
//    if (returnclose_parent == -1) {
//        print_error_and_exit("Couldn't close pipe to read from terminal in parent", errno);
//    }
//    poll_file_d[0].fd = STDIN_FILENO;
//    poll_file_d[0].events = POLLIN;
//    poll_file_d[1].fd = to_terminal[0];
//    poll_file_d[1].events = POLLIN | POLLHUP | POLLERR;
//}

//MARK: - Main function!

int main(int argc, char **argv) {
    
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"log", required_argument, 0, 'l'}, //change required argument here?
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    int option;
    while ((option = getopt_long(argc, argv, "p:l:d", long_options, NULL)) != -1 ) {
        switch(option) {
            case 'p':
                port_number = atoi(optarg);
                port_number_flag = 1;
                break;
            case 'l':
                logfile = optarg;
                log_flag = 1;
            case 'd':
                debug = 1;
                break;
            default:
                print_guidelines_and_exit();
        }
    }
    if (!port_number_flag) {
        print_guidelines_and_exit(); //cannot run without a port
    }
    if (debug) {
        printf("The following arguments are passed, port_number: %d, logfile=%s, debug: %d\n", port_number, logfile, debug);
    }
    save_terminal_state();
    set_program_terminal_state();
    atexit(restore_terminal_state);
//    read files as they are inputted into the keyboard
//    if (shell) {
//        if (debug) {
//            printf("entered shell option\n");
//        }
//        initialize_pipe(to_shell);
//        initialize_pipe(to_terminal);
//        secure_fork();
//        signal(SIGPIPE, signal_handler);
//        if (child_id == 0) { //if in child process, then do the shell stuff
//            if (debug) {
//                printf("in child process, processid: %d\n", getpid());
//            }
//            pre_shell_setup();
//            secure_shell();
//        } else {
//            if (debug) {
//                printf("in parent process, processid: %d\n", getpid());
//            }
//            pre_parent_setup();
//            int should_continue = 1;
//            while (should_continue != -1) {
//                should_continue = process_poll();
//            }
//        }
//    } else {
//        if (debug) {
//            printf("Entered default option");
//        }
//        ssize_t bytes_read = secure_read(STDIN_FILENO, buff, BUFFER_SIZE);
//        while (bytes_read > 0) {
//            //write
//            write_many(STDOUT_FILENO, buff, bytes_read, 0);
//            bytes_read = secure_read(STDIN_FILENO, buff, BUFFER_SIZE);
//        }
//    }
//    exit(SUCCESS_EXIT_CODE);
}
