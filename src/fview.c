#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include "file_table.h"
#include "procutils.h"

#define FILE_LISTENER_NAME "file-listener"

#define OPENED_PATH "/var/log/file-listener/opfiles" /* log file path for storing in disk opening events */
#define MODIFIED_PATH "/var/log/file-listener/modfiles" /* log file path for storing in disk modifying events */

static void printhelp(void) {
    /* ... */
}

static void emit_signal(void) {
    int listener_pid = getpid_by_name(FILE_LISTENER_NAME);
    if (listener_pid == -1) {
        fprintf(stderr, "Error: Couldnt emit signal to '%s'. -> %s\n", FILE_LISTENER_NAME, strerror(errno));
        fprintf(stderr, "If the error persist, try checking if the process is running.\n");
        return;
    }

    kill(listener_pid, SIGUSR1);
}

int main(int argc, char *argv[]) {
    int opt;

    int op, mod = 0;
    int big, small = 0;

    int verbose = 0;
    int range = 0;
    int help = 0;

    uint32_t n = 0;

    struct option long_ops[] = {
        {"opened", no_argument, NULL, 'o'},
        {"modified", no_argument, NULL, 'm'},
        {"biggest", no_argument, NULL, 'H'},
        {"smallest", no_argument, NULL, 'l'},
        {"range", required_argument, NULL, 'n'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "molHvn:h", long_ops, NULL)) != -1) {
        switch (opt) {
            case 'm': mod = 1; break;
            case 'o': op = 1; break;
            case 'H': big = 1; break;
            case 'l': small = 1; break;
            case 'n':
                char *endptr;
                long tmp = strtol(optarg, &endptr, 10);
                if (endptr == optarg) {
                    fprintf(stderr, "Error: Numeric value excepted when using flag '--range'.\n");
                    return 2;
                }

                if (tmp > UINT32_MAX)
                    tmp = UINT32_MAX;

                if (tmp <= 0)
                    tmp = 1;

                n = (uint32_t)tmp;

                break;
            case 'v': verbose = 1; break;
            case 'h': help = 1; break;
            default:
                fprintf(stderr, "Bad flag usage, '-%c' flag recieved.\n", opt);
                fprintf(stderr, "Use '--help' for more information.\n");
                return 2;
        }
    }

    if (help) {
        printhelp();
        return EXIT_SUCCESS;
    }

    if (!mod && !op)
        mod, op = 1;
    
    if (big && small)
        small = 0;

    emit_signal();

    return EXIT_SUCCESS;
}