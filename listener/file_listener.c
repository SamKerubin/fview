#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fanotify.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include "include/file_table.h"

/* path where log files are going to be saved and retrieved from */
#define OPENED_PATH "/var/log/opfiles"
#define MODIFIED_PATH "/var/log/modfiles"

#define BUFFER_SIZE 512
#define FILE_LINE_SIZE 1024

volatile sig_atomic_t running = 1;

struct _file *opened_table;
struct _file *modified_table;

struct pollfd fds[2];
int fan_fd;

/* for SIGTERM */
void handle_term(const int sig) {
    running = 0;
}

static char** splitstr(char *str, const char delimiter) {
    char **result;
    char *tmp = str;
    char *last_delim;

    char delim[2];
    delim[0] = delimiter;
    delim[1] = '\0';

    size_t count = 0;

    while (*tmp) {
        if (delimiter == *tmp) {
            count++;
            last_delim = tmp;
        }
        tmp++;
    }

    count += last_delim < (str + strlen(str) - 1);
    count++;

    result = malloc(sizeof(char *) * count);
    if (result == NULL) {
        return NULL;
    }

    size_t ind = 0;
    char *token = strtok(str, delim);
    while (token != NULL) {
        *(result + ind++) = strdup(token);
        token = strtok(0, delim);
    }

    *(result + ind) = '\0';

    return result;
}

static char* getfilepath(const int fd) {
    static char realpath[PATH_LENGTH];
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);

    ssize_t r = readlink(path, realpath, sizeof(realpath) - 1);
    if (r == -1)
        return NULL;

    realpath[r] = '\0';
    return realpath;
}

static struct _file* loadtable(const char* path) {
    struct _file *tmp_table = NULL;

    int fd = open(path, O_RDONLY | O_CREAT, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "file_listener Error: Couldnt load table from '%s'. -> %s", strerror(errno), path);
        return NULL;
    }

    char content[BUFFER_SIZE];
    char line[FILE_LINE_SIZE];
    ssize_t bytes_read;
    int line_pos = 0;

    while ((bytes_read = read(fd, content, sizeof(content)) > 0)) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (content[i] == '\n' || line_pos >= FILE_LINE_SIZE - 1) {
                line[line_pos] = '\0';
                char **splitted_line = splitstr(line, ':');
                if (splitted_line == NULL)
                    continue;

                char *key = splitted_line[0];
                char *value_str = splitted_line[1];
                char *endptr;
                int value = (int)strtol(value_str, &endptr, 10);

                additem(tmp_table, key, value);
            }
        }
    }

    close(fd);
    return tmp_table;
}

static int savetable(struct _file *table, const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "file_listener Error: Couldnt save table into '%s'. -> %s", path, strerror(errno));
        perror("open");
        return EXIT_FAILURE;
    }

    struct _file *item, *tmp;
    HASH_ITER(hh, table, item, tmp) {
        char *filename = item->key;
        char count = item->value;
        char entry[FILE_LINE_SIZE];

        snprintf(entry, sizeof(entry), "%s:%d", filename, count);
        ssize_t bytes_written = write(fd, entry, strlen(entry));
        if (bytes_written == -1) {
            syslog(LOG_ERR, "file_listener Error: Couldnt save table into '%s'. -> %s", path, strerror(errno));

            close(fd);
            return EXIT_FAILURE;
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}

// static void daemonize() {
//     pid_t pid = fork();

//     if (pid < 0) {
//         exit(EXIT_FAILURE);
//     }

//     if (pid > 0) {
//         exit(EXIT_SUCCESS);
//     }

//     umask(0);
//     if (chdir("/") < 0) {
//         perror("chdir");
//         exit(EXIT_FAILURE);
//     }

//     close(STDIN_FILENO);
//     close(STDOUT_FILENO);
//     close(STDERR_FILENO);

//     if (setsid() < 0) {
//         exit(EXIT_FAILURE);
//     }

//     int fd = open("/dev/null", O_RDWR);
//     dup2(fd, STDIN_FILENO);
//     dup2(fd, STDOUT_FILENO);
//     dup2(fd, STDERR_FILENO);
// }

static void set_timer(void) {
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd == -1) {
        syslog(LOG_ERR, "file_listener Error: Couldnt set timerfd timer. -> %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct itimerspec spec;
    spec.it_interval.tv_sec = 5;
    spec.it_interval.tv_nsec = 0;
    spec.it_value.tv_sec = 5;
    spec.it_value.tv_nsec = 0;

    if (timerfd_settime(tfd, 0, &spec, NULL) == -1) {
        perror("timerfd_settime");
        exit(EXIT_FAILURE);
    }

    fds[0].fd = fan_fd;
    fds[0].events = POLLIN;
    fds[1].fd = tfd;
    fds[1].events = POLLIN;
}

static void init_fanotify(const char *path) {
    fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) {
        syslog(LOG_ERR, "file_listener Error: Couldnt initialize fanotify. -> %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (fanotify_mark(fan_fd,
                        FAN_MARK_ADD | FAN_MARK_MOUNT,
                        FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD,
                        AT_FDCWD,
                        path) == -1) {
        syslog(LOG_ERR, "file_listener Error: Couldnt mark mount point to fanotify in '%s' -> %s", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static int path_in_blacklist(const char* path) {
    /* this will be getting better in the future */
    return strcmp(path, OPENED_PATH) == 0        || 
            strcmp(path, MODIFIED_PATH) == 0     ||
            strncmp(path, "/proc/", 6) == 0      ||
            strncmp(path, "/dev/", 6) == 0       ||
            strncmp(path, "/sys/", 6) == 0       ||
            strncmp(path, "/run/", 6) == 0;
}

static void loop(void) {
    while(running) {
        int ret = poll(fds, 1, -1);
        if (ret == -1) {
            perror("poll");
            continue;
        }
        
        if (fds[0].events & POLLIN) {
            struct fanotify_event_metadata buffer[200];
            ssize_t len = read(fan_fd, buffer, sizeof(buffer));

            if (len < 0 && errno != EAGAIN) {
                syslog(LOG_ERR, "file_listener Error: Couldnt read event metadata from file descriptior '%d'. -> %s", fan_fd, strerror(errno));
                break;
            }

            if (len <= 0) 
                continue;

            struct fanotify_event_metadata *meta;
            struct _file *tmp_table = NULL;
            char *tmp_path = NULL;
            for (meta = buffer; FAN_EVENT_OK(meta, len); meta = FAN_EVENT_NEXT(meta, len)) {
                if (meta->mask & FAN_OPEN || meta->mask & FAN_MODIFY) {
                    const char *filepath = getfilepath(meta->fd);
                    if (filepath == NULL) {
                        close(meta->fd);
                        continue;
                    }

                    if (path_in_blacklist(filepath))
                        continue;

                    if (meta->mask & FAN_OPEN) {
                        tmp_table = opened_table;
                        tmp_path = OPENED_PATH;
                    } else {
                        tmp_table = modified_table;
                        tmp_path = MODIFIED_PATH;
                    }

                    struct _file *file_affected = getitem(tmp_table, filepath);
                    int count = file_affected == NULL ? 1 : file_affected->value + 1;
                    additem(tmp_table, filepath, count);

                }

                tmp_table = NULL;
                tmp_path = NULL;

                close(meta->fd);
            }
        }

        if (fds[1].events & POLLIN) {
            savetable(opened_table, OPENED_PATH);
            savetable(modified_table, MODIFIED_PATH);
        }

    }
}

static void clean_loop(void) {
    syslog(LOG_INFO, "file_listener Daemon has stopped.");
    savetable(opened_table, OPENED_PATH);
    savetable(modified_table, MODIFIED_PATH);
    clear_table(opened_table);
    clear_table(modified_table);
    closelog();
    close(fan_fd);
}

int main(void) {
    // daemonize();

    signal(SIGTERM, handle_term);

    openlog("file_listener", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "file_listener Daemon started.");

    set_timer();

    opened_table = loadtable(OPENED_PATH);
    modified_table = loadtable(MODIFIED_PATH);

    init_fanotify("/");

    loop();
    clean_loop();

    return EXIT_SUCCESS;
}
