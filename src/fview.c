#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdint.h>
#include "file_table.h"
#include "procutils.h"
#include "fileutils.h"
#include "strutils.h"

#define FILE_LISTENER_NAME "file-listener"

#define OPENED_PATH "/var/log/file-listener/opfiles" /* log file path for storing in disk opening events */
#define MODIFIED_PATH "/var/log/file-listener/modfiles" /* log file path for storing in disk modifying events */

struct match {
    struct _file **table;
    char *searching;
    const char *readingpath;
};

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

static void handle_match(char *line, void *arg) {
    /* ... */
}

static int search_matches(struct match *mt, const char *path) {
    mt->readingpath = path;
    return readfile(path, handle_match, mt);
}

int main(int argc, char *argv[]) {
    int opt;

    /* if neither of op or mod are active, returns all the matches found without filtering */
    int op, mod = 0;
    int big, small = 0;

    int metadata = 0;
    int verbose = 0;
    int range = 0;
    int help = 0;

    uint32_t n = 0;

    struct option long_ops[] = {
        {"opened", no_argument, NULL, 'o'},
        {"modified", no_argument, NULL, 'm'},
        {"biggest", no_argument, NULL, 'M'},
        {"smallest", no_argument, NULL, 'l'},
        {"range", required_argument, NULL, 'n'},
        {"verbose", no_argument, NULL, 'v'},
        {"show-metadata", no_argument, NULL, 'a'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };
    
    while ((opt = getopt_long(argc, argv, "molMvan:h", long_ops, NULL)) != -1) {
        switch (opt) {
            case 'm': mod = 1; break;
            case 'o': op = 1; break;
            case 'M': big = 1; break;
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
            case 'a': metadata = 1; break;
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

    if (optind >= argc) {
        fprintf(stderr, "Directory path expected.\n");
        return EXIT_FAILURE;
    }

    if (big && small)
        small = 0;
    
    emit_signal();

    char *dirpath = argv[optind];

    struct _file *table;

    struct match mt = {
        .table = &table,
        .searching = dirpath,
    };

    search_matches(&mt, OPENED_PATH);
    search_matches(&mt, MODIFIED_PATH);

/**
 * search matches ->
 * 4 possible conditions (max of 3):
 * - most/least modified   
 * - most/least opened
 * 
 * first save entries that matches <dirpath> variable
 * then filter them with the conditions (based on the flags)
 * right before printing the general information, checks if the flag -a is active
 * if the flag -a is active, prints all metadata of the file
 */

    return EXIT_SUCCESS;
}