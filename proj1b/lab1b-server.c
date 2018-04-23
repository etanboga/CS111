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
#include "zlib.h"

#define BUFFER_SIZE 256
#define CHUNK 16384

int debug = 0;
int compress_flag = 0;
int port_number_flag = 0;
int port_number;
int newsockfd;
int to_shell[2]; //reads terminal and outputs to shell
int to_terminal[2]; //reads shell and outputs to terminal
pid_t child_id;

struct pollfd poll_file_d[2];

//define constants for exit state

const int FAIL_EXIT_CODE = 1;
const int SUCCESS_EXIT_CODE = 0;
const nfds_t NUM_FDS = 2;


//MARK: - compress / decompress functions

int compress_Ege(void* from, void* to, int from_size, int to_size, int level) {
    z_stream strm;
    int return_def;
    int bytes_compressed = -1;
    strm.total_in = strm.avail_in = from_size;
    strm.total_out = strm.avail_out = to_size;
    strm.next_in = (Bytef *) from;
    strm.next_out = (Bytef *) to;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    return_def = deflateInit(&strm, level);
    if (return_def != Z_OK) {
        deflateEnd(&strm);
        return bytes_compressed;
    } else {
        return_def = deflate(&strm, Z_FINISH);
        if (return_def == Z_STREAM_END) {
            bytes_compressed = (int) strm.total_out;
        } else {
            deflateEnd(&strm);
            return bytes_compressed;
        }
    }
    deflateEnd(&strm);
    return bytes_compressed;
}

int decompress_Ege(void* from, void* to, int from_size, int to_size) {
    z_stream strm;
    int return_inf;
    int bytes_decompressed = -1;
    strm.total_in = strm.avail_in = from_size;
    strm.total_out = strm.avail_out = to_size;
    strm.next_in = (Bytef *) from;
    strm.next_out = (Bytef *) to;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    return_inf = inflateInit(&strm);
    if (return_inf != Z_OK) {
        inflateEnd(&strm);
        if (debug) {
            printf("Error: couldn't initialize inflate");
        }
        return bytes_decompressed;
    } else {
        return_inf = inflate(&strm, Z_FINISH);
        if (return_inf == Z_STREAM_END) {
            bytes_decompressed = (int) strm.total_out;
        } else {
            if (debug) {
                printf("Error: inflate doesn't return Z_FINISH\n");
            }
            if (return_inf == Z_DATA_ERROR) {
                if (debug) {
                    printf("experienced Z_DATA_ERROR\n");
                }
            }
            inflateEnd(&strm);
            return bytes_decompressed;
        }
    }
    inflateEnd(&strm);
    return bytes_decompressed;
}

//MARK: - print functions

void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(FAIL_EXIT_CODE);
}

void print_guidelines_and_exit() {
    printf("Usage: ./lab1b-server --port=portnumber --compress --debug \n");
    exit(FAIL_EXIT_CODE);
}

void cleanup_shell() {
    int status;
    pid_t returnpid = waitpid(child_id, &status, 0);
    if (returnpid == -1) {
        print_error_and_exit("Error waiting for child", errno);
    } else if (debug) {
        printf("exiting shell properly in process id: %d \n", getpid());
    }
    fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
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
            case 0x03:
                if (is_to_shell) {
                    if (debug) {
                        printf("Killing shell");
                    }
                    kill(child_id, SIGINT);
                }
                break;
            case 0x04: //for ^D
                if (is_to_shell) {
                    int returnclose = close(to_shell[1]); //don't write to shell
                    if (returnclose == -1) {
                        print_error_and_exit("Couldn't close pipe to shell in ^D", errno);
                    }
                    if (debug) {
                        printf("Exiting program using option ^D\n");
                    }
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
    } else if (debug) {
        printf("Fork successfull");
    }
}

void pre_shell_setup() {
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
    dupcheck = dup2(to_terminal[1], STDOUT_FILENO);
    if (dupcheck == -1) {
        print_error_and_exit("error duplicating output in child", errno);
    }
    dupcheck = dup2(to_terminal[1], STDERR_FILENO);
    if (dupcheck == -1) {
        print_error_and_exit("Error duplicating standard error in child", errno);
    }
    returnclose_child = close(to_shell[0]);
    if (returnclose_child == -1) {
        print_error_and_exit("Couldn't close pipe to read terminal in child", errno);
    }
    returnclose_child = close(to_terminal[1]);
    if (returnclose_child == -1) {
        print_error_and_exit("Couldn't close pipe to write to terminal in child", errno);
    }
}

void secure_shell() {
    char *const argv[] = {"/bin/bash", NULL};
    int execreturn = execv("/bin/bash", argv);
    if (execreturn == -1) {
        print_error_and_exit("Exec of bash failed", errno);
    }
    if (debug) {
        printf("Executing bash shell");
    }
}

void pre_parent_setup() {
    int returnclose_parent = close(to_terminal[1]); //don't need to write to terminal
    if (returnclose_parent == -1) {
        print_error_and_exit("Couldn't close pipe to write to terminal in parent", errno);
    }
    returnclose_parent = close(to_shell[0]); //don't need to read from terminal
    if (returnclose_parent == -1) {
        print_error_and_exit("Couldn't close pipe to read from terminal in parent", errno);
    }
    poll_file_d[0].fd = newsockfd;
    poll_file_d[0].events = POLLIN | POLLHUP | POLLERR;
    poll_file_d[1].fd = to_terminal[0];
    poll_file_d[1].events = POLLIN | POLLHUP | POLLERR;
}

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
        //we have input coming from newsockfd, write to shell
        char buffer_socket[BUFFER_SIZE];
        ssize_t bytes_read = read(newsockfd, buffer_socket, BUFFER_SIZE);
        
        if (bytes_read <= 0) {
            if (debug) {
                printf("Error: problem when reading from client");
            }
            kill(child_id, SIGTERM);
            cleanup_shell();
            should_continue_loop = -1;
            return should_continue_loop;
        } else {
            if (compress_flag) {
                if (debug) {
                    printf("Decompressing before writing input to shell");
                }
                char decompressed_buffer[CHUNK];
                
                int bytes_decompressed = decompress_Ege(buffer_socket, decompressed_buffer, BUFFER_SIZE, CHUNK);
                if (bytes_decompressed == -1) {
                    print_error_and_exit("Error: couldn't decompress from client before writing to shell", errno);
                }
                write_many(to_shell[1], decompressed_buffer, bytes_decompressed, 1);
            } else {
                write_many(to_shell[1], buffer_socket, bytes_read, 1); //send to shell
            }
        }
    }
    if (poll_file_d[1].revents & POLLIN) {
        //we can read from the shell. communicate to client via newsockfd
        char buffer_shell[BUFFER_SIZE];
        ssize_t bytes_read = secure_read(poll_file_d[1].fd, buffer_shell, BUFFER_SIZE);
        if (bytes_read == 0) {
            cleanup_shell();
            should_continue_loop = -1;
            return should_continue_loop;
        } else {
            if (compress_flag) {
                if (debug) {
                    printf("Compressing input from shell before sending it off to client");
                }
                char compressed_buffer[CHUNK];
                int bytes_compressed = compress_Ege(buffer_shell, compressed_buffer, BUFFER_SIZE, CHUNK, Z_DEFAULT_COMPRESSION);
                if (bytes_compressed == -1) {
                    print_error_and_exit("Error: couldn't compress from shell before sending over to client", errno);
                }
                write_many(newsockfd, compressed_buffer, bytes_compressed, 0);
            } else {
            write_many(newsockfd, buffer_shell, bytes_read, 0); //send to socket
            }
        }
    }
    if (poll_file_d[1].revents & (POLLERR | POLLHUP)) {
        //shell shut down
        if (debug) {
            printf("Stop writing to shell");
        }
        int returnclose = close(to_shell[1]); //stop writing to shell
        if (returnclose == -1) {
            print_error_and_exit("Couldn't close writing to shell in POLLERR", errno);
        }
        cleanup_shell();
        should_continue_loop = -1;
        return should_continue_loop;
    }
    return should_continue_loop;
}

void signal_handler(int num_signal) {
    if (debug) {
        printf("In signal handler");
    }

    if (num_signal == SIGPIPE) {
        if (debug) {
            printf("in sigpipe");
        }
        cleanup_shell();
    }
    exit(SUCCESS_EXIT_CODE);
}

void setup_signal_handlers() {
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        print_error_and_exit("couldn't set up SIGTERM handler", errno);
    }
    if (signal(SIGPIPE, signal_handler) == SIG_ERR) {
        print_error_and_exit("couldn't set up SIGPIPE handler", errno);
    }
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        print_error_and_exit("couldn't set up SIGINT handler", errno);
    }
    if (debug) {
        printf("successfully set up signal handlers");
    }
}

//MARK: - Main function!

int main(int argc, char **argv) {
    setup_signal_handlers();
    struct sockaddr_in serv_addr, cli_addr;
    unsigned int clilen;
    int sockfd;
    static struct option long_options[] = {
        {"port", required_argument, 0, 'p'},
        {"debug", no_argument, 0, 'd'},
        {"compress", no_argument, 0, 'c'},
        {0, 0, 0, 0}
    };
    int option;
    while ((option = getopt_long(argc, argv, "p:cd", long_options, NULL)) != -1 ) {
        switch(option) {
            case 'p':
                port_number = atoi(optarg);
                port_number_flag = 1;
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
    if (!port_number_flag) {
        print_guidelines_and_exit(); //cannot run without a port
    }
    if (debug) {
        printf("The following arguments are passed, port_number: %d, compress-flag: %d, debug: %d\n", port_number, compress_flag, debug);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        print_error_and_exit("Error: couldn't open socket", errno);
    } else if (debug) {
        printf("set up initial socket");
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_number);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        print_error_and_exit("Error: couldn't bind initial server", errno);
    } else if (debug) {
        printf("Successfully binded");
    }
    if (listen(sockfd, 5) < 0) {
        print_error_and_exit("Error: no socket found", errno);
    } else if (debug) {
        printf("listen worked properly");
    }
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (newsockfd < 0) {
        print_error_and_exit("Error: couldn't accept connection", errno);
    } else if (debug) {
        printf("Successfully accepted connection");
    }
    initialize_pipe(to_shell);
    initialize_pipe(to_terminal);
    secure_fork();
    if (child_id == 0) { //we are in child process, do shell setup
        if (debug) {
            printf("in child process, processid: %d\n", getpid());
        }
        pre_shell_setup();
        secure_shell();
    } else {
        if (debug) {
            printf("in parent process, processid: %d\n", getpid());
        }
        pre_parent_setup();
        int should_continue = 1;
        while (should_continue != -1) {
            should_continue = process_poll();
        }
    }
    exit(SUCCESS_EXIT_CODE);
}
