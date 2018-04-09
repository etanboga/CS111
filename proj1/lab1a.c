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

struct termios default_terminal_state;
struct termios program_terminal_state;

//for debugging option

int debug = 0;
int pipe_write_shell[2];
int pipe_write_terminal[2];
pid_t child_id;

//define constants for exit state

const int FAIL_EXIT_CODE = 1;
const int SUCCESS_EXIT_CODE = 0;

//buffer with recommended size in the spec


const int BUFFER_SIZE = 256;
char buff[BUFFER_SIZE];

//MARK: - print functions

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(FAIL_EXIT_CODE);
}

void print_guidelines_and_exit() {
    printf("Usage: ./lab1a.c --shell --debug \n");
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
    } else if (debug) {
        printf("restored terminal state properly in process id: %d \n", getpid());
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
    atexit(restore_terminal_state);
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

void write_many(int fd, char* buffer, size_t size, int debug) {
    for (size_t i = 0; i < size; i++) {
        char* current_char_ptr  = (buffer+i);
        switch(*current_char_ptr) {
            case 0x04: //for ^D
                restore_terminal_state();
                if (debug) {
                    printf("Exiting program using option ^D\n");
                }
                exit(0);
                break;
            case 0x0D: //for \r -> Carriage return
            case 0x0A: { //for \n -> newline
                if (debug) {
                    printf("Entered option for EOF\n");
                }
                char eofchars[2];
                eofchars[0] = '\r';
                eofchars[1] = '\n';
                secure_write(fd, eofchars, 2);
            }
            break;
            default:
                secure_write(fd, current_char_ptr, 1);
        }
        
    }
}

//MARK: - shell option functions

void initialize_pipe(int pipefd[2]) {
    int pipereturn = pipe(pipefd);
    if (pipereturn == -1) {
        print_error_and_exit("Couldn't create pipe", errno);
    }
    if (debug) {
        printf("initialized pipe %p\n", pipefd);
    }
}

void secure_fork() {
    child_id = fork();
    if (child_id == -1) {
        print_error_and_exit("Fork failed", errno);
    }
}

//MARK: - Main function!

int main(int argc, char **argv) {
    
    static struct option long_options[] = {
        {"shell", no_argument, 0, 's'},
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    int shell = 0;
    int option;
    while ((option = getopt_long(argc, argv, "sd", long_options, NULL)) != -1 ) {
        switch(option) {
            case 's':
                shell = 1;
                break;
            case 'd':
                debug = 1;
                break;
            default:
                print_guidelines_and_exit();
        }
    }
    if (debug && shell) {
        printf("shell and debug arguments are passed, shell: %d, debug: %d\n", shell, debug);
    }
    save_terminal_state();
    set_program_terminal_state();
    //read files as they are inputted into the keyboard
    if (shell) {
        if (debug) {
            printf("entered shell option\n");
        }
        initialize_pipe(pipe_write_shell);
        initialize_pipe(pipe_write_terminal);
        secure_fork();
        if (child_id == 0) {
            if (debug) {
                printf("in child process, processid: %d\n", getpid());
            }
        } else {
            if (debug) {
                printf("in parent process, processid: %d\n", getpid());
                wait(&child_id);
            }
        }
    } else {
        ssize_t bytes_read = secure_read(STDIN_FILENO, buff, BUFFER_SIZE);
        while (bytes_read > 0) {
            //write
            write_many(STDOUT_FILENO, buff, bytes_read, debug);
            bytes_read = secure_read(STDIN_FILENO, buff, BUFFER_SIZE);
        }
    }
    exit(0);
}
