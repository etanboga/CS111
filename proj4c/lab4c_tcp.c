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
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <mraa/aio.h>
#define BUFFERSIZE 256

//Addition for Lab4c

int sockfd, portno, n;
struct sockaddr_in serveraddr;
struct hostent* server;
char* hostname;


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
char* logfile = NULL;
char* id_string = NULL;
int log_fd;
int can_continue = 1; //checks if our output should continue, set to false if stop read in commands


void print_error_and_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(EXIT_FAILURE);
}

//added for lab4c

void connection_error_exit(char* error_message, int error_no) {
    fprintf(stderr, "%s, error = %d, message = %s \n", error_message, error_no, strerror(error_no));
    exit(2);
}

void print_guidelines_and_exit() {
    printf("Usage: ./lab4c_tls  portnumber --id=xxxxxxxxxx --host=name/address --log=filename --period=# --scale=C/F \n");
    exit(EXIT_FAILURE);
}

ssize_t secure_write(int fd, void* buffer, size_t size) {
    ssize_t bytes_written = write(fd, buffer, size);
    if (bytes_written == -1) {
        print_error_and_exit("Error writing bytes", errno);
    }
    return bytes_written;
}

void* secure_malloc(size_t size) {
    void* return_ptr = malloc(size);
    if (return_ptr == NULL) {
        print_error_and_exit("Error in malloc", errno);
    }
    return return_ptr;
}

void get_command(char* command) {
    unsigned long length = strlen(command);
    if (strcmp(command, "SCALE=F") == 0) {
        is_fahrenheit = 1;
    } else if (strcmp(command, "SCALE=C") == 0) {
        is_fahrenheit = 0;
    } else if (strcmp(command, "STOP") == 0) {
        can_continue = 0;
    } else if (strcmp(command, "START") == 0) {
        can_continue = 1;
    } else if (strcmp(command, "OFF") == 0) {
        run_flag = 0;
    } else if (memcmp(command, "PERIOD=", 7) == 0 && length > 7) {
        char* period_string = secure_malloc((length - 7) * sizeof(char));
        memcpy(period_string, &command[7], length - 7);
        int new_period = atoi(period_string);
        if (new_period > 0) {
            period = new_period;
        }
        free(period_string);
    } else if (memcmp(command, "LOG ", 4) != 0 || length < 5) { //if it is not log or if it is log but there is no line of text
        fprintf(stdout, "Invalid commad entered");
        return;
    }
    if (log_flag) {
        secure_write(log_fd, command, length);
        secure_write(log_fd, "\n" , 1);
    }
}

char input_buffer[BUFFERSIZE];
char commands_buffer[BUFFERSIZE];
int command_begin = 0; //tracks current position in commands_buffer
int current_index = 0; //points to the beginning of the command being processed



void process_input() {
    ssize_t bytes_read = read(sockfd, input_buffer, BUFFERSIZE);
    if (bytes_read < 0) {
        print_error_and_exit("Couldn't read input from keyboard", errno);
    }
    int i;
    for (i = 0; i < bytes_read; i++) {
        if (input_buffer[i] == '\n') { //need to process command
            char* current_command = secure_malloc((current_index - command_begin + 1) * sizeof(char));
            int it;
            int comm_index = 0;
            for(it = command_begin; it < current_index; it++){
                current_command[comm_index] = commands_buffer[it % BUFFERSIZE];
                comm_index++;
            }
            current_command[comm_index]='\0';
            get_command(current_command);
            free(current_command);
            command_begin = current_index;
        } else {
            commands_buffer[current_index % BUFFERSIZE] = input_buffer[i]; //if we don't need to process the command, store characters in commands buffer
            current_index++;
        }
    }
}


int main(int argc, char * argv[]) {
    static struct option long_options[] = {
        {"period", required_argument, 0, 'p'},
        {"scale", required_argument, 0, 's'},
        {"log", required_argument, 0, 'l'},
        {"id", required_argument, 0, 'i'},
        {"host", required_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int option;
    while ((option = getopt_long(argc, argv, "p:s:l:i:h", long_options, NULL)) != -1 ) {
        switch(option) {
                case 'p':
                period_flag = 1;
                period = atoi(optarg);
                break;
                case 's':
                if (strlen(optarg) == 1 && optarg[0] == 'C') {
                    is_fahrenheit = 0;
                } else if (strlen(optarg) == 1 && optarg[0] == 'F') {
                    is_fahrenheit = 1;
                } else {
                    print_guidelines_and_exit();
                }
                case 'l':
                log_flag = 1;
                logfile = optarg;
                break;
                case 'i':
                id_string = optarg;
                break;
                case 'h':
                hostname = optarg;
                break;
            default:
                print_guidelines_and_exit();
        }
    }
    if (strlen(hostname) == 0 || strlen(logfile) == 0) {
        print_guidelines_and_exit();
    }
    if (log_flag) {
        log_fd = open(logfile, O_NONBLOCK | O_WRONLY | O_APPEND | O_CREAT);
        if (log_fd < 0) {
            print_error_and_exit("log file cannot be opened", errno);
        }
    }
    
    if (period <= 0) {
        print_error_and_exit("Cannot have negative period", EXIT_FAILURE);
    }
    portno = atoi(argv[optind]);
    if (portno <= 1024) {
        print_error_and_exit("Error: invalid port number", errno);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        connection_error_exit("Error: cannot open socket", errno);
    }
    server = gethostbyname(hostname);
    if (server == NULL) {
        connection_error_exit("Error: couldn't connect to server", errno);
    }
    bzero((char * ) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char * ) server->h_addr, (char *) & serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr* ) &serveraddr, sizeof(serveraddr))  < 0) {
        connection_error_exit("Error: cannot connect", errno);
    }
    
    char id_buffer[20];
    int bytes_read = sprintf(id_buffer,"ID=%s\n",id_string);
    secure_write(sockfd, id_buffer, bytes_read);
    if (log_flag) {
        secure_write(log_fd, id_buffer, bytes_read);
    }
    //MRAA Initialization
    
    mraa_aio_context temp;
    temp = mraa_aio_init(1);
    signal(SIGINT, do_when_interrupted);
    struct pollfd fds[1];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN|POLLHUP|POLLERR;
    int pollreturn;
    struct timeval clock;
    struct tm *now;
    while ((pollreturn = poll(fds, NUM_FDS, 0)) >= 0) {
        if (can_continue) {
            gettimeofday(&clock, 0);
            int a = mraa_aio_read(temp);
            //temperature calculation
            float R = 1023.0/a - 1.0;
            R = R0*R;
            float temperature = 1.0/(log(R/R0)/B+1/298.15)-273.15;
            if (is_fahrenheit) {
                temperature = (9.0/5)*temperature + 32;
            }
            char output[70];
            now = localtime(&(clock.tv_sec));
            int bytes_sent = sprintf(output, "%02d:%02d:%02d %.1f\n", now->tm_hour, now->tm_min, now->tm_sec,temperature);
            if (bytes_sent < 0) {
                print_error_and_exit("Cannot sprintf temperature", errno);
            }
            secure_write(sockfd, output, bytes_sent);
            if (log_flag) {
                secure_write(log_fd, output, bytes_sent);
            }
        }
        if (fds[0].revents & POLLIN) {
            process_input();
        }
        int i;
        int end_loop = 0;
        for (i = 0; i < period; i++) {
            if (!run_flag) {
                gettimeofday(&clock, 0);
                char output[70];
                now = localtime(&(clock.tv_sec));
                int bytes_sent = sprintf(output, "%02d:%02d:%02d SHUTDOWN\n",now->tm_hour, now->tm_min, now->tm_sec);
                if (bytes_sent < 0) {
                    print_error_and_exit("Cannot sprintf shutdown", errno);
                }
                secure_write(sockfd, output, bytes_sent);
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
    close(sockfd);
    exit(EXIT_SUCCESS);
}

