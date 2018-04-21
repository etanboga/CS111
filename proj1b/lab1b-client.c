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
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <zlib.h>

#define BUFFER_SIZE 256
#define NUM_FDS 2
#define DIR_SERVER 1
#define DIR_CLIENT 0
#define COMPRESSION_BUFFER_SIZE 1024

struct termios default_terminal_state;
struct termios program_terminal_state;
z_stream stdin_to_shell;
z_stream shell_to_stdout;


int debug = 0;
int sockfd;
int logfd;
int logflag = 0;
int compress_flag = 0;
int port_number_flag = 0;
int port_number = 0;
struct pollfd poll_file_d[2];



//define constants for exit state

const int FAIL_EXIT_CODE = 1;
const int SUCCESS_EXIT_CODE = 0;

//MARK: - print functions

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(FAIL_EXIT_CODE);
}

void print_guidelines_and_exit() {
    printf("Usage: ./lab1b-client --port=portnumber --log=logfile --debug \n");
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

void write_many(int fd, char* buffer, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        char* current_char_ptr  = (buffer+i);
        switch(*current_char_ptr) {
            case 0x0D: //for \r -> Carriage return
            case 0x0A:
                if (fd != STDOUT_FILENO) { //if writing to log file, write as newline
                    char newline[1];
                    newline[0] = '\n';
                    secure_write(fd,newline, 1);
                }
                else { //for \n -> newline, if writing to stdout do the proper transformation
                    char eofchars[2] = {'\r', '\n'};
                    secure_write(fd, eofchars, 2);
                }
                break;
            default:
                secure_write(fd, current_char_ptr, 1);
        }
    }
}

//MARK: - logging the transfers

void log_transfer(char* buffer, size_t size, int direction) {
    char newline = '\n';
    if (direction == DIR_CLIENT) {
        int return_write = dprintf(logfd, "RECEIVED %zu bytes: ", size);
        if (return_write < 0) {
            print_error_and_exit("Error: couldn't write to log file while sending", errno);
        }
        write_many(logfd, buffer, size);
        secure_write(logfd, &newline, size);
    }
    if (direction == DIR_SERVER) {
        int return_write = dprintf(logfd, "SENT %zu bytes: ", size);
        if (return_write < 0) {
            print_error_and_exit("Error: couldn't write to log file while receiving", errno);
        }
        write_many(logfd, buffer, size);
        secure_write(logfd, &newline, size);
    }
}

//MARK: - polling

int process_poll() {
    int should_continue_loop = 1;
    int pollresult = poll(poll_file_d, NUM_FDS, 0);
    if (pollresult == -1) {
        print_error_and_exit("Error setting up poll", errno);
    }
    //if none of the file descriptors are ready, return for next iteration
    if (pollresult == 0) {
        return should_continue_loop;
    }
    if (poll_file_d[0].revents & POLLIN) {
        if (debug) {
            printf("input from stdin in process_poll");
        }
        //we can read from standard input
        char buffer_stdin[BUFFER_SIZE];
        ssize_t bytes_read = secure_read(poll_file_d[0].fd, buffer_stdin, BUFFER_SIZE);
        write_many(STDOUT_FILENO, buffer_stdin, bytes_read); //write out to stdout
        if (compress_flag) {
            if (debug) {
                printf("in compress option");
            }
            stdin_to_shell.zalloc = Z_NULL;
            stdin_to_shell.zfree = Z_NULL;
            stdin_to_shell.opaque = Z_NULL;
            char compressed_buffer[COMPRESSION_BUFFER_SIZE];
            int return_def = deflateInit(&stdin_to_shell, Z_DEFAULT_COMPRESSION);
            if (return_def != Z_OK) {
                print_error_and_exit("Error: couldn't deflateInit", errno);
            }
            stdin_to_shell.avail_in = BUFFER_SIZE;
            stdin_to_shell.next_in = (Bytef *) buffer_stdin;
            stdin_to_shell.avail_out = COMPRESSION_BUFFER_SIZE;
            stdin_to_shell.next_out = (Bytef *) compressed_buffer;
            do {
                if (debug) {
                    printf("Compressing with deflate");
                }
                deflate(&stdin_to_shell, Z_SYNC_FLUSH);
            } while (stdin_to_shell.avail_in > 0);
            write_many(sockfd, compressed_buffer, COMPRESSION_BUFFER_SIZE - stdin_to_shell.avail_out);
            if (logflag) {
                log_transfer(compressed_buffer, COMPRESSION_BUFFER_SIZE - stdin_to_shell.avail_out, DIR_SERVER);
            }
        } else {
            if (debug) {
                printf("Without compress option");
            }
            if (logflag) {
                log_transfer(buffer_stdin, bytes_read, DIR_SERVER);
            }
            write_many(sockfd, buffer_stdin, bytes_read);
        }
    }
    if (poll_file_d[1].revents & POLLIN) {
        //reading input from server
        char buffer_server[BUFFER_SIZE];
        ssize_t bytes_read = secure_read(poll_file_d[1].fd, buffer_server, BUFFER_SIZE);
        if (bytes_read == 0) {
            if (debug) {
                printf("nothing read from server in client side");
            }
            int returnclose = close(sockfd);
            if (returnclose == -1) {
                print_error_and_exit("Error: couldn't close socket", errno);
            }
            exit(SUCCESS_EXIT_CODE);
        } else {
            if (debug) {
                printf("received data from server");
            }
            if (compress_flag) {
                if (debug) {
                    printf("Need to decompress here");
                }
            }
            write_many(STDOUT_FILENO, buffer_server, bytes_read);
            if (logflag) {
                log_transfer(buffer_server, bytes_read, DIR_CLIENT);
            }
        }
    }
    if (poll_file_d[1].revents & (POLLERR | POLLHUP)) {
        //server is going away
        if (debug) {
            printf("Server is going away: POLLERR / POLLHUP");
        }
        int returnclose = close(sockfd); //stop writing to shell
        if (returnclose == -1) {
            print_error_and_exit("Couldn't close writing to shell in POLLERR", errno);
        }
        exit(SUCCESS_EXIT_CODE);
    }
    return should_continue_loop;
}

void pre_poll_setup() {
    poll_file_d[0].fd = STDIN_FILENO;
    poll_file_d[0].events = POLLIN | POLLHUP | POLLERR;
    poll_file_d[1].fd = sockfd;
    poll_file_d[1].events = POLLIN | POLLHUP | POLLERR;
}

//MARK: - Main function!

int main(int argc, char **argv) {
    struct sockaddr_in serv_addr;
    struct hostent *server;
    static struct option long_options[] = {
        { "log", required_argument, 0, 'l'},
        {"port", required_argument, 0, 'p'},
        {"compress", no_argument, 0, 'c'},
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    char *logfile = NULL;
    int option;
    while ((option = getopt_long(argc, argv, "l:p:cd", long_options, NULL)) != -1 ) {
        switch(option) {
            case 'l':
                logfile = optarg;
                logflag = 1;
                break;
            case 'p':
                port_number_flag = 1;
                port_number = atoi(optarg);
                break;
            case 'c':
                compress_flag = 1;
                break;
            case 'd':
                debug = 1;
                break;
            default:
                print_guidelines_and_exit();
        }
    }
    if (debug) {
        printf("The following arguments are passed, port_number: %d, debug: %d, logflag: %d, logfile: %s, compress flag: %d \n", port_number, debug, logflag, logfile, compress_flag);
    }
    if (logflag) {
        logfd = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (logfd == -1) {
            print_error_and_exit("couldn't create log file", errno);
        }
    }
    save_terminal_state();
    set_program_terminal_state();
    atexit(restore_terminal_state);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        print_error_and_exit("Error: couldn't open socket", errno);
    }
    else if (debug) {
        printf("Successfully opened socket");
    }
    server = gethostbyname("localhost");
    if ( server == NULL ) {
        print_error_and_exit("Error: host not found", errno);
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *) &serv_addr.sin_addr.s_addr, (char *) server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port_number);
    int connect_fail;
    connect_fail = (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0);
    if (connect_fail) {
        print_error_and_exit("Error: couldn't connect", errno);
    } else if (debug) {
        printf("Connected to server");
    }
    pre_poll_setup();
    int should_continue = 1;
    while (should_continue != -1) {
        should_continue = process_poll();
    }
    exit(SUCCESS_EXIT_CODE);
}

