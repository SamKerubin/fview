#define _POSIX_C_SOURCE 200809L /* lstat, strdup */
#include <stdio.h> /* fprintf, stderr, stdout */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE, malloc, realloc, free */
#include <fcntl.h> /* all the macros starting with O */
#include <unistd.h> /* open, read, write, close */
#include <getopt.h> /* getopt_long, no_argument, optind, option */
#include <sys/stat.h> /* stat, S_ISDIR */
#include <string.h> /* strerror, snprintf, strcmp, strcpy, strlen */
#include <errno.h> /* errno */

#define BUFFER_SIZE 1024

#define BLACKLIST_PATH "/var/log/file-listener/file_listener.blacklist" /* file path for the blacklist file */

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

/* reads the blacklist and returns a malloc'ed array of the paths read */
static char** readblacklist(int fd, char *dirpath, char **list, size_t *count, int *found_path) {
    if (list == NULL) {
        list = (char **)malloc(sizeof(char *));
        if (list == NULL) {
            perror("malloc");
            return NULL;
        }
    }

    char content[BUFFER_SIZE];
    char line[FILENAME_MAX];
    ssize_t bytes_read;
    int line_pos = 0;

    while ((bytes_read = read(fd, content, sizeof(content))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (content[i] == '\n' || line_pos >= FILENAME_MAX - 1) {
                line[line_pos] = '\0';

                if (strcmp(line, dirpath) == 0) {
                    *found_path = 1;
                    break;
                }

                char **tmp = (char **)realloc(list, sizeof(char *) * (*count + 1));
                if (tmp == NULL) {
                    perror("realloc");
                    return NULL;
                }

                tmp[*count] = (char *)malloc((sizeof(char) * strlen(line)) + 1);
                if (tmp[*count] == NULL) {
                    perror("malloc");
                    return NULL;
                }

                strcpy(tmp[*count], line);
                list = tmp;
                (*count)++;

                line_pos = 0;
            } else {
                line[line_pos++] = content[i];
            }
        }
    }

    return list;
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

    int flags = O_RDWR | O_CREAT;
    if (!remove) flags |= O_APPEND;

    int fd = open(BLACKLIST_PATH, flags, 0644);
    if (fd == -1) {
        fprintf(stderr, "Error: Couldnt open blacklist file. -> %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    char **list = NULL;
    size_t count = 0;
    int found_path = 0;

    if (verbose) fprintf(stdout, "Reading blacklist looking for '%s'.\n", dirpath);

    if (!(list = readblacklist(fd, dirpath, list, &count, &found_path))) {
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

        int trunc_fd = open(BLACKLIST_PATH, O_WRONLY | O_TRUNC, 0644);
        close(fd);

        for (size_t i = 0; i < count; i++) {
            ssize_t bytes_written = write(trunc_fd, list[i], strlen(list[i]));
            if (bytes_written == -1) {
                fprintf(stderr, "Error: Couldnt save new paths into file. -> %s\n", strerror(errno));
                clear_list(list, count);
                return EXIT_FAILURE;
            }
        }
        close(trunc_fd);

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
        if (write(fd, buff, sizeof(buff)) == -1) {
            fprintf(stderr, "Error: Couldnt append path into blacklist content. -> %s\n", strerror(errno));
            clear_list(list, count);
            return EXIT_FAILURE;
        }

        if (verbose) fprintf(stdout, "'%s' path appended to blacklist.\n", dirpath);
    }

    clear_list(list, count);
    close(fd);
    return EXIT_SUCCESS;
}