//Name: Ege Tanboga
//email: ege72282@gmail.com
//ID: 304735411

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

void print_guidelines_and_exit() {
    printf("Usage: lab0 --input file1 --output file2 --segfault --catch");
    exit(1);
}

void cause_segfault() {
    char *ptr  = NULL;
    *ptr = 'e';
}

void catch_segfault(int num) {
    if (num == SIGSEGV) {
        fprintf(stderr, "Segmentation fault: error = %d, message = %s \n", errno, strerror(errno));
        exit(4);
    }
}

void copyContent(int fd1, int fd2) {
    char buffer;
    while (read(fd1, &buffer, sizeof(char))) {
        write(fd2, &buffer, sizeof(char));
    }
}

int main(int argc, char **argv) {
    static struct option long_options[] = {
        { "input", required_argument, 0, 'i'},
        { "output", required_argument, 0, 'o'},
        { "segfault", no_argument, 0, 's' },
        {"catch", no_argument, 0, 'c'},
        {0, 0, 0, 0}
    };
    char *inputfile = NULL;
    char *outputfile = NULL;
    int segfault = 0;
    int catch = 0;
    int option, fd_input, fd_output;
    //repeatedly parse arguments
    while ((option = getopt_long(argc, argv, "i:o:sc", long_options, NULL)) != -1) {
        switch(option) {
            case 'i':
                inputfile = optarg;
                break;
            case 'o':
                outputfile = optarg;
                break;
            case 's':
                segfault = 1;
                break;
            case 'c':
                catch = 1;
                break;
            default:
                print_guidelines_and_exit();
                
        }
    }
    if (catch) {
        signal(SIGSEGV, catch_segfault);
    }
    if (segfault) {
        cause_segfault();
    }
    if (inputfile) {
      fd_input = open(inputfile, O_RDONLY);
        if (fd_input >= 0 ) {
            close(0);
            dup(fd_input);
            close(fd_input);
        } else {
        fprintf(stderr, "Error opening file: error = %d, message = %s \n", errno, strerror(errno));
        exit(2);
	}
    }
    
    if (outputfile) {
        fd_output = creat(outputfile, 0666);
        if (fd_output >= 0 ) {
            close(1);
            dup(fd_output);
            close(fd_output);
        } else {
        fprintf(stderr, "Error creating output file: error = %d, message = %s, \n", errno, strerror(errno));
	exit(3);
	}
    }
    //since 0 is standard input and 1 is standard output we can do the necessary transformation
    copyContent(0, 1);
    exit(0);
}
