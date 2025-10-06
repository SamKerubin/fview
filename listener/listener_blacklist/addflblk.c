#include <stdio.h> /* fprintf, stderr */
#include <stdlib.h> /* EXIT_SUCCESS, EXIT_FAILURE */
#include <fcntl.h> /* all the macros starting with O */
#include <unistd.h> /* open, read, write, close */
#include <getopt.h> /* getopt_long, no_argument, optind */
#include <sys/stat.h> /* lstat, stat */
#include <string.h> /* strerror */
#include <errno.h> /* errno */

#define BUFFER_SIZE 1024

#define BLACKLIST_PATH "/etc/file_listener.blacklist" /* file path for the blacklist file */

int main(int argc, char *argv[]) {
    int opt;

    struct option long_ops[] = {
        {"remove", no_argument, NULL, 'r'},
        {"show-list", no_argument, NULL, 's'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'help'},
        {0, 0, 0, 0}
    };

    int remove = 0;
    int show_list = 0;
    int verbose = 0;
    int help = 0;

    while((opt = getopt_long(argc, argv, "rsvh", long_ops, NULL)) != -1) {
        switch (opt) {
            case 'r': remove = 1; break;
            case 's': show_list = 1; break;
            case 'v': verbose = 1; break;
            case 'h': help = 1; break;
            default:
                fprintf(stderr, "Bad flag usage, '-%c' recieved.\nUse --help for more detailed information.\n", opt);
                return 2;
        }
    }

    if (help) {
        /* prints help message */
        return EXIT_SUCCESS;
    }

    if (optind >= argc) {
        fprintf(stderr, "Directory path expected.\n");
        return EXIT_FAILURE;
    }

    char *dirpath = argv[optind];

    int fd = open(BLACKLIST_PATH, O_RDWR | (O_APPEND & (O_APPEND >> remove)) | O_CREAT, 0644);
    if (fd == -1) {
        fprintf(stderr, "Error: Couldnt open blacklist file. -> %s", strerror(errno));
        return EXIT_FAILURE;
    }

    char content[BUFFER_SIZE];
    char line[FILENAME_MAX];
    ssize_t bytes_read;
    int line_pos = 0;

    char **list;

    if (remove) {
        /* read content, check if path exists, remove if exits, do nothing if not */
    } else {
        /* read content, check if path exists, append if not, do nothing if exists */
    }

    if (show_list) {
        /* show every item in the blacklist */
    }

    return EXIT_SUCCESS;
}