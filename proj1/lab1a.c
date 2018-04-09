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

int debug = 0;
int shell = 0;
int to_shell[2]; //reads terminal and outputs to shell
int to_terminal[2]; //reads shell and outputs to terminal
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
    }
    if (shell) {
        int status;
        pid_t returnpid = waitpid(child_id, &status, 0);
        if (returnpid == -1) {
            print_error_and_exit("Error waiting for child", errno);
        } else if (debug) {
            printf("restored terminal state properly in process id: %d \n", getpid());
        }
    }
    else if (debug) {
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
            case 0x03:
                printf("pressed control C, should kill shell here");
                break;
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

void secure_shell() {
    int execreturn = execv("/bin/bash", NULL);
    if (execreturn == -1) {
        print_error_and_exit("Exec of bash failed", errno);
    }
    if (debug) {
        printf("Executing bash shell");
    }
}

//MARK: - Main function!

int main(int argc, char **argv) {
    
    static struct option long_options[] = {
        {"shell", no_argument, 0, 's'},
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
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
        initialize_pipe(to_shell);
        initialize_pipe(to_terminal);
        secure_fork();
        if (child_id == 0) {
            if (debug) {
                printf("in child process, processid: %d\n", getpid());
            }
            int returnclose_child = close(to_shell[1]); //don't need to write to shell
            if (returnclose_child == -1) {
                print_error_and_exit("Couldn't close pipe to write to shell in child", errno);
            }
            if (debug) {
                printf("Closed pipe to write to terminal in child");
            }
            returnclose_child = close(to_terminal[0]); //don't need to read from shell
            if (returnclose_child == -1) {
                print_error_and_exit("Couldn't close pipe to read from shell in child", errno);
            }
            if (debug) {
                printf("Closed pipe to read from shell in child");
            }
            int dupcheck;
            dupcheck = dup2(to_shell[0], STDIN_FILENO); //read terminal and write to terminal
            if (dupcheck == -1) {
                print_error_and_exit("error duplicating input in child", errno);
            }
//            dupcheck = dup2(to_terminal[1], STDOUT_FILENO);
//            if (dupcheck == -1) {
//                print_error_and_exit("error duplicating output in child", errno);
//            }
//            dupcheck = dup2(to_terminal[1], STDERR_FILENO);
//            if (dupcheck == -1) {
//                print_error_and_exit("Error duplicating standard error in child", errno);
//            }
//            returnclose_child = close(to_shell[0]);
//            if (returnclose_child == -1) {
//                print_error_and_exit("Couldn't close pipe to read terminal in child", errno);
//            }
//            returnclose_child = close(to_terminal[1]);
//            if (returnclose_child == -1) {
//                print_error_and_exit("Couldn't close pipe to write to terminal in child", errno);
//            }
            secure_shell();
        } else {
            if (debug) {
                printf("in parent process, processid: %d\n", getpid());
            }
            int returnclose_parent = close(to_terminal[1]); //don't need to write to terminal
            if (returnclose_parent == -1) {
                print_error_and_exit("Couldn't close pipe to write to terminal in parent", errno);
            }
            returnclose_parent = close(to_shell[0]); //don't need to read from terminal
            if (returnclose_parent == -1) {
                print_error_and_exit("Couldn't close pipe to read from terminal in parent", errno);
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
