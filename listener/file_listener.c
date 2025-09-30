#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fanotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "uthash.h"

#define OPENED_PATH "/var/log/opfiles"
#define MODIFIED_PATH "/var/log/modfiles"

#define PATH_LENGTH 4096

typedef struct _file {
    char filename[4096];
    int count;
    UT_hash_handle hh;
} _file;

static char* getfilepath(int fd) {
    char path[PATH_LENGTH];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);

    char realpath[PATH_LENGTH];
    ssize_t r = readlink(path, realpath, sizeof(realpath) - 1);
    if (r != -1) {
        realpath[r] = '\0';
    }

    return realpath;
}

int main(void) {
    __pid_t pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);

    while(1) {
        int fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY | O_LARGEFILE);
        if (fan_fd == -1) {
            perror("fanotify_init");
            exit(EXIT_FAILURE);
        }

        if (fanotify_mark(fan_fd,
                            FAN_MARK_ADD | FAN_MARK_MOUNT,
                            FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD,
                            AT_FDCWD,
                            "/") == -1) {
            perror("fanotify_mark");
            exit(EXIT_FAILURE);
        }

        for (;;) {
            struct fanotify_event_metadata buffer[200];
            ssize_t len = read(fan_fd, buffer, sizeof(buffer));

            if (len < 0 && errno != EAGAIN) {
                perror("read");
                break;
            }

            if (len <= 0) 
                continue;

            struct fanotify_event_metadata *meta;
            for (meta = buffer; FAN_EVENT_OK(meta, len); meta = FAN_EVENT_NEXT(meta, len)) {
                if (meta->mask & FAN_OPEN) {
                    char realpath[PATH_LENGTH];
                    strcpy(realpath, getfilepath(meta->fd));
                }

                if (meta->mask & FAN_MODIFY) {
                    char realpath[PATH_LENGTH];
                    strcpy(realpath, getfilepath(meta->fd));
                }

                close(meta->fd);
            }
        }
    }

    return EXIT_SUCCESS;
}