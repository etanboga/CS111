//
//  main.c
//  proj1a
//
//  Created by Ege Tanboga on 4/7/18.
//  Copyright © 2018 Tanbooz. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

void print_guidelines_and_exit() {
    printf("Usage: ./lab1a.c --shell --debug");
    exit(1);
}

int main(int argc, char **argv) {
    
    //before anything, grab current terminal options and save it somewhere
    //change to new terminal options
    //options should be restored after all returned input has been processed
    static struct option long_options[] = {
        {"shell", no_argument, 0, 's'},
        {"debug", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };
    int shell = 0, debug = 0;
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
        printf("shell and debug arguments are passed, shell: %d, debug: %d", shell, debug);
    }
    return 0;
}
