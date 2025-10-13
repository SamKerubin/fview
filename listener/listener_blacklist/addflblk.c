#define _POSIX_C_SOURCE 200809L /* lstat, strdup */
#include <stdio.h> /* fprintf, stderr, stdout */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, malloc, realloc, free, calloc */
#include <fcntl.h> /* all the macros starting with O */
#include <unistd.h> /* open, read, write, close */
#include <getopt.h> /* getopt_long, no_argument, optind, option */
#include <sys/stat.h> /* stat, S_ISDIR */
#include <string.h> /* strerror, snprintf, strcmp, strcpy, strlen */
#include <errno.h> /* errno */
#include "../include/fileutils.h" /* readfile, savefile, appendline */

#define BUFFER_SIZE 1024

#define BLACKLIST_PATH "/var/log/file-listener/file_listener.blacklist" /* file path for the blacklist file */

struct blacklist_element {
    char *path;
    char ***list;
    size_t *count;
    int *found;
};

static void printhelp() {
    /* prints help message */
}

/* clears allocated directory paths in list */
static void clear_list(char **list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}   

static void blacklist_handler(char *line, void *arg) {
    struct blacklist_element *blk = (struct blacklist_element *)arg;

    if (strcmp(line, blk->path) == 0) {
        *(blk->found) = 1;
        return;
    }

    char **tmp = (char **)realloc(*(blk->list), sizeof(char *) * (*(blk->count) + 1));
    if (tmp == NULL) {
        perror("realloc");
        return;
    }

    tmp[*(blk->count)] = (char *)malloc((sizeof(char) * strlen(line)) + 1);
    if (tmp[*(blk->count)] == NULL) {
        perror("malloc");
        return;
    }

    strcpy(tmp[*(blk->count)], line);
    *(blk->list) = tmp;
    (*(blk->count))++;

}

/* reads the blacklist and returns a malloc'ed array of the paths read */
static int readblacklist(char *dirpath, char ***list, size_t *count, int *found_path) {
    if (*list == NULL) {
        *list = (char **)calloc(1, sizeof(char *));
        if (*list == NULL) {
            perror("malloc");
            return 0;
        }
    }

    struct blacklist_element blk = {
        .path = dirpath,
        .list = list,
        .count = count,
        .found = found_path
    };

    if (!readfile(BLACKLIST_PATH, blacklist_handler, &blk)) {
        return 0;
    }

    return 1;
}

int main(int argc, char *argv[]) {
    int opt;

    struct option long_ops[] = {
        {"remove", no_argument, NULL, 'r'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    int remove = 0;
    int verbose = 0;
    int help = 0;

    while((opt = getopt_long(argc, argv, "rvh", long_ops, NULL)) != -1) {
        switch (opt) {
            case 'r': remove = 1; break;
            case 'v': verbose = 1; break;
            case 'h': help = 1; break;
            default:
                fprintf(stderr, "Bad flag usage, '-%c' recieved.\nUse --help for more detailed information.\n", opt);
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

    char *dirpath = argv[optind];

    struct stat st_buf;
    if (lstat(dirpath, &st_buf) != 0) {
        fprintf(stderr, "Couldnt read state of the file or directory '%s'. -> %s\n", dirpath, strerror(errno));
        return EXIT_FAILURE;
    }

    if (!S_ISDIR(st_buf.st_mode)) {
        fprintf(stderr, "Directory path expected. Please insert a directory path to continue.\n");
        return EXIT_FAILURE;
    }

    char **list = NULL;
    size_t count = 0;
    int found_path = 0;

    if (verbose) fprintf(stdout, "Reading blacklist looking for '%s'.\n", dirpath);

    readblacklist(dirpath, &list, &count, &found_path);
    if (!list) {
        fprintf(stderr, "Error: Couldnt read blacklist content. -> %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (remove) {
        if (!found_path) {
            fprintf(stderr, "'%s' is not registered in the blacklist.\n", dirpath);
            clear_list(list, count);
            return EXIT_FAILURE;
        }

        if (verbose) {
            fprintf(stdout, "Path found.\n");
            fprintf(stdout, "Trying to remove...\n");
        }

        if (!savefile(BLACKLIST_PATH, list, 1)) {
            fprintf(stderr, "Error: Couldnt remove path from blacklist. -> %s\n" ,strerror(errno));
            clear_list(list, count);
            return EXIT_FAILURE;
        }

        if (verbose) fprintf(stdout, "'%s' path removed from blacklist.\n", dirpath);
    } else {
        if(found_path) {
            fprintf(stderr, "'%s' is already in the blacklist.\n", dirpath);
            clear_list(list, count);
            return EXIT_FAILURE;
        }

        if (verbose) {
            fprintf(stdout, "Not found.\n");
            fprintf(stdout, "Trying to append path to blacklist...\n");
        }

        char buff[strlen(dirpath) + 2];
        snprintf(buff, sizeof(buff), "%s\n", dirpath);

        if (!appendline(BLACKLIST_PATH, buff, 0)) {
            fprintf(stderr, "Error: Couldnt add path to blacklist. -> %s", strerror(errno));
            return EXIT_FAILURE;
        }

        if (verbose) fprintf(stdout, "'%s' path appended to blacklist.\n", dirpath);
    }

    clear_list(list, count);
    return EXIT_SUCCESS;
}