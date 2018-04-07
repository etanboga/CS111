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
                printf("Passed shell option, shell flag: %d", shell);
                break;
            case 'd':
                debug = 1;
                printf("Passed debug option, debug flag: %d", debug);
                break;
            default:
                print_guidelines_and_exit();
        }
    }
    return 0;
}
