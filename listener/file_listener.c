#define _GNU_SOURCE /* AT_FDCWD */
#include <stdio.h> /* perror, snprintf, ssize_t */
#include <stdlib.h> /* exit, EXIT_SUCCESS, EXIT_FAILURE, malloc, strtol */
#include <unistd.h> /* readlink, read, write, close */
#include <sys/fanotify.h> /* fanotify_init, fanotify_mark, fanotify_event_metadata, all the macros starting with FAN */
#include <sys/stat.h>
#include <syslog.h> /* syslog, openlog, closelog, all the macros starting with LOG */
#include <time.h> /* time */
#include <fcntl.h> /* open */
#include <string.h> /* strtok, strdup, strerror, strcmp, strncmp, strlen */
#include <signal.h> /* signal, SIGTERM */
#include <errno.h> /* errno */
#include <poll.h> /* poll, pollfd, POLLIN */
#include "include/file_table.h" /* _file, additem, getitem, clean_table, PATH_LENGTH, HASH_ITER */

#define OPENED_PATH "/var/log/opfiles" /* log file path for storing in disk opening events */
#define MODIFIED_PATH "/var/log/modfiles" /* log file path for storing in disk modifying events */

#define INTERVAL_SEC 5 /* timout for each time the process saves data */

volatile sig_atomic_t running = 1; /* flag for the main loop */

struct _file *opened_table; /* stores the opening events in memory */
struct _file *modified_table; /* stores the modifying events in memory */

int fan_fd; /* file descriptor of fanotify events */
struct pollfd fds[1]; /* poll if fanotify recieves an event */

/* handles SIGTERM, setting running flag to 0 */
void handle_term(const int sig) {
    syslog(LOG_INFO, "Signal %s, recieved, stopping process.", strsignal(sig));
    running = 0;
}
/* splits a string str using a delimiter
    returns a string array where each element is a string token
*/
static char** splitstr(char *str, const char delimiter) {
    char **result;
    char *tmp = str;
    char *last_delim = NULL;

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

    if (last_delim != NULL && last_delim < (str + strlen(str) - 1))
        count++;
    else if (last_delim == NULL)
        count = 1;

    count += last_delim < (str + strlen(str) - 1);
    count++;

    result = malloc(sizeof(char *) * count + 1);
    if (result == NULL) {
        return NULL;
    }

    size_t ind = 0;
    char *token = strtok(str, delim);
    while (token != NULL) {
        *(result + ind++) = strdup(token);
        token = strtok(0, delim);
    }

    *(result + ind) = NULL;
    return result;
}

/* given a file descriptor fd, returns the real file path of that descriptor */
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

/* loads the table to be used in run-time
    returns the content of the saved table, or NULL if its empty
*/
static struct _file* loadtable(const char* path) {
    struct _file *tmp_table = NULL;

    int fd = open(path, O_RDONLY | O_CREAT, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Error: Couldnt load table from '%s'. -> %s", strerror(errno), path);
        return NULL;
    }

    char line[PATH_LENGTH];
    ssize_t bytes_read;
    int line_pos = 0;

    while ((bytes_read = read(fd, line, sizeof(line))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (line[i] == '\n' || line_pos >= PATH_LENGTH - 1) {
                line[line_pos] = '\0';
                char **splitted_line = splitstr(line, ':');
                if (splitted_line == NULL || splitted_line[0] == NULL || splitted_line[1] == NULL) {
                    line_pos = 0;
                    continue;
                }

                char *key = splitted_line[0];
                char *value_str = splitted_line[1];
                char *endptr;
                long value = strtol(value_str, &endptr, 10);

                additem(&tmp_table, key, value);
                line_pos = 0;
            }
            line_pos++;
        }
    }

    close(fd);
    return tmp_table;
}

/* saves current data into disk */
static int savetable(struct _file *table, const char *path) {
    if (table == NULL) {
        return EXIT_FAILURE;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Error: Couldnt save table into '%s'. -> %s", path, strerror(errno));
        perror("open");
        return EXIT_FAILURE;
    }

    struct _file *item, *tmp;
    HASH_ITER(hh, table, item, tmp) {
        char *filename = item->key;
        long count = item->value;
        char entry[PATH_LENGTH];

        snprintf(entry, sizeof(entry), "%s:%ld\n", filename, count);
        ssize_t bytes_written = write(fd, entry, strlen(entry));
        if (bytes_written == -1) {
            syslog(LOG_ERR, "Error: Couldnt save table into '%s'. -> %s", path, strerror(errno));

            close(fd);
            return EXIT_FAILURE;
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}

/* initalize fanotify in a specific path */
static void init_fanotify(const char *path) {
    fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) {
        syslog(LOG_ERR, "Error: Couldnt initialize fanotify. -> %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (fanotify_mark(fan_fd,
                        FAN_MARK_ADD | FAN_MARK_MOUNT,
                        FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD,
                        AT_FDCWD,
                        path) == -1) {
        syslog(LOG_ERR, "Error: Couldnt mark mount point to fanotify in '%s' -> %s", path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    fds[0].fd = fan_fd;
    fds[0].events = POLLIN;
}

/* checks if a path is inside the process blacklist (paths it must ignore) */
static int path_in_blacklist(const char *path) {
    /* this will be getting better in the future */
    return strcmp(path, OPENED_PATH) == 0        || 
            strcmp(path, MODIFIED_PATH) == 0     ||
            strncmp(path, "/proc/", 6) == 0      ||
            strncmp(path, "/dev/", 6) == 0       ||
            strncmp(path, "/sys/", 6) == 0       ||
            strncmp(path, "/run/", 6) == 0;
}

/* main loop of the process */
static void loop(void) {
    time_t last_save = time(NULL);
    while(running) {
        int ret = poll(fds, 1, 1000);
        if (ret == -1) {
            perror("poll");
            continue;
        }

        time_t now = time(NULL);
        if (now - last_save >= INTERVAL_SEC) {
            savetable(opened_table, OPENED_PATH);
            savetable(modified_table, MODIFIED_PATH);
            last_save = now;
        }

        if (ret > 0 && fds[0].revents & POLLIN) {
            struct fanotify_event_metadata buffer[200];
            ssize_t len;

            do {
                len = read(fan_fd, buffer, sizeof(buffer));
                if (len <= 0) break;

                if (len < 0 && errno != EAGAIN) {
                    syslog(LOG_ERR, "Error: Couldnt read event metadata from file descriptior '%d'. -> %s", fan_fd, strerror(errno));
                    break;
                }

                struct fanotify_event_metadata *meta;
                for (meta = buffer; FAN_EVENT_OK(meta, len); meta = FAN_EVENT_NEXT(meta, len)) {
                    if (meta->mask & FAN_OPEN || meta->mask & FAN_MODIFY) {
                        const char *filepath = getfilepath(meta->fd);
                        if (filepath == NULL) {
                            close(meta->fd);
                            continue;
                        }

                        if (path_in_blacklist(filepath)) {
                            close(meta->fd);
                            continue;
                        }

                        struct _file **tmp_table = (meta->mask & FAN_OPEN) ? &opened_table : &modified_table;
                        char *tmp_path = (meta->mask & FAN_OPEN) ? OPENED_PATH : MODIFIED_PATH;

                        struct _file *file_affected = getitem(tmp_table, filepath);
                        long count = file_affected ? file_affected->value + 1L : 1L;
                        additem(tmp_table, filepath, count);
                    }
                    close(meta->fd);
                }
            } while (len > 0);
        }
    }
}

/* cleans the process memory after finishing the main loop */
static void clean_loop(void) {
    syslog(LOG_INFO, "Daemon has stopped.");
    savetable(opened_table, OPENED_PATH);
    savetable(modified_table, MODIFIED_PATH);
    clear_table(&opened_table);
    clear_table(&modified_table);
    close(fan_fd);
}

int main(void) {
    signal(SIGTERM, handle_term);

    openlog("file_listener", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon has started.");

    opened_table = loadtable(OPENED_PATH);
    modified_table = loadtable(MODIFIED_PATH);
    
    init_fanotify("/");

    loop();
    clean_loop();

    closelog();
    return EXIT_SUCCESS;
}
