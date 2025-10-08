/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/fview/tree/main/listener/file_listener.c
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE /* AT_FDCWD */
#include <stdio.h> /* perror, snprintf, ssize_t, remove */
#include <stdlib.h> /* exit, EXIT_SUCCESS, EXIT_FAILURE, malloc, strtol */
#include <unistd.h> /* readlink, open, read, write, close */
#include <sys/fanotify.h> /* fanotify_init, fanotify_mark, fanotify_event_metadata, all the macros starting with FAN */
#include <sys/stat.h> /* mkdir, stat */
#include <syslog.h> /* syslog, openlog, closelog, all the macros starting with LOG */
#include <time.h> /* time */
#include <fcntl.h> /* creat, all the macros starting with O */
#include <string.h> /* strtok, strdup, strerror, strcmp, strncmp, strlen */
#include <signal.h> /* sigaction, sigemptyset, sa_handler, SIGTERM, SIGKILL */
#include <errno.h> /* errno */
#include <poll.h> /* poll, pollfd, POLLIN */
#include "include/file_table.h" /* _file, additem, getitem, clean_table, PATH_LENGTH, HASH_ITER */

#define OPENED_PATH "/var/log/file-listener/opfiles" /* log file path for storing in disk opening events */
#define MODIFIED_PATH "/var/log/file-listener/modfiles" /* log file path for storing in disk modifying events */
#define BLACKLIST_PATH "/var/log/file-listener/file_listener.blacklist" /* file path for the blacklist file */

#define TMP_OPENED_PATH "/tmp/file-listener/opfiles/%u.tmp" /* temporary log file for storing in disk opening events */
#define TMP_MODIFIED_PATH "/tmp/file-listener/modfiles/%u.tmp" /* temporary log file for storing in disk modifying events */

#define MAX_TMP_FILES 500 /* max temporary log files that can be created */
#define MAX_TMP_SIZE 2000 /* max items a temporary file can store before opening a new temporary file */

#define INTERVAL_SEC 5 /* timout for each time the process saves data */

#define BUFFER_SIZE 1024 /* size for buffers */

volatile sig_atomic_t running = 1; /* flag for the main loop */

struct _file *opened_table; /* stores the opening events in memory */
struct _file *modified_table; /* stores the modifying events in memory */

u_int count_opening_tmp = 1; /* counter for the current amount of opening temporary files created */
u_int count_modifying_tmp = 1; /* counter for the current amount of modifying temporary files created */

u_int current_opening_size = 0; /* counter for the items the current opening temporary file has stored */
u_int current_modifying_size = 0; /* counter for the itmes the current modifying temporary file has stored */

int fan_fd; /* file descriptor of fanotify events */
struct pollfd fds[1]; /* poll if fanotify recieves an event */

/**
 * @brief signal handling
 *  
 * when a signal is recieved, it updates the value of variable running to 0.
 * this makes the function loop to stop after the next iteration.
 *  
 * @param sig number of the signal recieved.
 * 
 */
void handle_term(const int sig) {
    syslog(LOG_INFO, "Signal %s, recieved, stopping process.", strsignal(sig));
    running = 0;
}

/**
 * @brief splits a string using a delimiter
 *  
 * given a string, counts how many times a delimiter appears and
 * splits the string using strtok
 *  
 * @param str the string that is going to be splitted
 * @param delimitar character that acts as the splitter
 * @return alloc'ed array of the splitted tokens of the string
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

    result = malloc((sizeof(char *) * count) + 1);
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

/**
 * @brief returns a file path
 *  
 * given a file descriptor fd, reads the link from '/proc/self/fd/...'
 * to return the path of the file descriptor
 *  
 * @param fd file descriptor
 * @return real file path of a file descriptor
 */
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

/**
 * @brief loads a file for storing its data
 *  
 * loads a file and parses its content
 * each line parsed will be stored as a new item in the table
 * in the process, it modifies the items in the table with each item added
 * 
 * @param path path of the file that is going to be read
 * @param table table struct thats going to be updated with the items added
 * @return exit code (0, 1)
 */
static int loadtable(const char* path, struct _file **table) {
    int fd = open(path, O_RDONLY | O_CREAT, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Error: Couldnt load table from '%s'. -> %s", path, strerror(errno));
        return EXIT_FAILURE;
    }

    char line[PATH_LENGTH];
    char content[BUFFER_SIZE];
    ssize_t bytes_read;
    int line_pos = 0;

    while ((bytes_read = read(fd, content, sizeof(content))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (content[i] == '\n' || line_pos >= PATH_LENGTH - 1) {
                line[line_pos] = '\0';

                char **splitted_line = splitstr(line, ':');
                if (splitted_line == NULL || splitted_line[0] == NULL || splitted_line[1] == NULL) {
                    line_pos = 0;
                    continue;
                }

                char key[PATH_LENGTH];
                char value_str[PATH_LENGTH];
                strcpy(key, splitted_line[0]);
                strcpy(value_str, splitted_line[1]);

                char *endptr;
                long value = strtol(value_str, &endptr, 10);

                additem(table, key, value);
                for (size_t i = 0; splitted_line[i] != NULL; i++) {
                    free(splitted_line[i]);
                }

                free(splitted_line);

                line_pos = 0;
            } else {
                line[line_pos++] = content[i];
            }
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}

/**
 * @brief saves a struct into disk
 *  
 * saves the current content of a table into a file
 * truncates the file and re-writes the new content into it
 *  
 * @param table the struct that is going to be saved into disk
 * @param path path of the file that is going to be truncated
 * @return exit code (0, 1)
 * 
 */
static int savetable(struct _file **table, const char *path) {
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
    HASH_ITER(hh, *table, item, tmp) {
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

/**
 * @brief merge all temporary files created into one single table
 *  
 * using a pre-formatted path, formats it up to tmp_count
 * and reads its content, after loading everything, 
 * saves the table into a permanent file path
 *  
 * @param path temporary file path
 * @param dest_path path where all the temporary files are going to be stored
 * @param tmp_count count of how many temporary files are there
 * @return exit code (0, 1)
 */
static int mergetmp(const char *path, const char *dest_path, u_int tmp_count) {
    struct _file *merged_table = NULL;
    for (u_int i = 1; i <= tmp_count; i++) {
        char realpath[PATH_LENGTH];
        snprintf(realpath, sizeof(realpath), path, i);
        
        loadtable(realpath, &merged_table);
        remove(realpath);
    }

    int r = savetable(&merged_table, dest_path);
    clear_table(&merged_table);

    return r;
}

/* clears the content of both tables */
static void clean_tables(void) {
    clear_table(&opened_table);
    clear_table(&modified_table);
}

/** 
 * @brief initalize fanotify in a specific path
 *  
 * given a path, tries to initialize fanotify on that path
 * with the flags FAN_OPEN and FAN_MODIFY
 * 
 * @param path path where fanotify is going to be setted up
*/
static void init_fanotify(const char *path) {
    fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) {
        syslog(LOG_ERR, "Error: Couldnt initialize fanotify. -> %s", strerror(errno));
        clean_tables();
        exit(EXIT_FAILURE);
    }

    if (fanotify_mark(fan_fd,
                        FAN_MARK_ADD | FAN_MARK_MOUNT,
                        FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD,
                        AT_FDCWD,
                        path) == -1) {
        syslog(LOG_ERR, "Error: Couldnt mark mount point to fanotify in '%s' -> %s", path, strerror(errno));
        clean_tables();
        exit(EXIT_FAILURE);
    }

    fds[0].fd = fan_fd;
    fds[0].events = POLLIN;
}

/**
 * @brief returns if a path its inside a blacklist
 *  
 * opens the path '/var/log/file-listener/file_listener.blacklist'
 * and reads its content looking for the existence of path
 *  
 * @param path path that is going to be checked if its inside the blacklist
 * @return exit code (0, 1)
 */
static int path_in_blacklist(const char *path) {
    /* paths that must be ignored regardless the blacklist file content */
    if (strcmp(path, OPENED_PATH) == 0                  || 
            strcmp(path, MODIFIED_PATH) == 0            ||
            strcmp(path, BLACKLIST_PATH) == 0           ||
            strncmp(path, TMP_OPENED_PATH, 21) == 0     ||
            strncmp(path, TMP_MODIFIED_PATH, 21) == 0   ||
            strncmp(path, "/proc/", 6) == 0             ||
            strncmp(path, "/dev/", 6) == 0              ||
            strncmp(path, "/sys/", 6) == 0              ||
            strncmp(path, "/run/", 6) == 0)
        return 1;

    int fd = open(BLACKLIST_PATH, O_RDONLY | O_CREAT, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Error: Unable to read blacklist file. -> %s", strerror(errno));
        return -1;
    }

    char line[PATH_LENGTH];
    char content[BUFFER_SIZE];
    ssize_t bytes_read;
    int line_pos = 0;

    while ((bytes_read = read(fd, content, sizeof(content))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (content[i] == '\n' || line_pos >= PATH_LENGTH - 1) {
                line[line_pos] = '\0';

                if (strncmp(path, line, strlen(line)) == 0) {
                    close(fd);
                    return 1;
                }

                line_pos = 0;
            } else {
                line[line_pos++] = content[i];
            }
        }
    }

    close(fd);
    return 0;
}

/** 
 * @brief main loop of the process 
 *  
 * starts recording events using fanotify and poll
 *  
 * each time an event is registered, saves it in a table
 *  
 * when reached a certain amount of saves, loads all the data to a permanent file
 * */
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
            char current_oppath[PATH_LENGTH];
            char current_modpath[PATH_LENGTH];

            snprintf(current_oppath, sizeof(current_oppath), TMP_OPENED_PATH, count_opening_tmp);
            snprintf(current_modpath, sizeof(current_modpath), TMP_MODIFIED_PATH, count_modifying_tmp);

            savetable(&opened_table, current_oppath);
            savetable(&modified_table, current_modpath);
            last_save = now;
        }

        if (ret < 0 || !(fds[0].revents & POLLIN))
            continue;

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
                if (!(meta->mask & FAN_OPEN) && !(meta->mask & FAN_MODIFY)) {
                    close(meta->fd);
                    continue;
                }

                const char *filepath = getfilepath(meta->fd);
                if (filepath == NULL) {
                    close(meta->fd);
                    continue;
                }

                if (path_in_blacklist(filepath)) {
                    close(meta->fd);
                    continue;
                }

                struct _file **table = (meta->mask & FAN_OPEN) ? &opened_table : &modified_table;
                u_int *content_count = (meta->mask & FAN_OPEN) ? &current_opening_size : &current_modifying_size;
                
                struct _file *file_affected = getitem(table, filepath);
                
                long count = file_affected ? file_affected->value + 1L : 1L;
                additem(table, filepath, count);
                
                if (*content_count < MAX_TMP_SIZE) {
                    (*content_count)++;
                    close(meta->fd);
                    continue;
                }

                u_int *file_count = (meta->mask & FAN_OPEN) ? &count_opening_tmp : &count_modifying_tmp;

                char *path = (meta->mask & FAN_OPEN) ? TMP_OPENED_PATH : TMP_MODIFIED_PATH;
                char current_path[PATH_LENGTH];
                snprintf(current_path, sizeof(current_path), path, *file_count);

                savetable(table, current_path);
                clear_table(table);

                *content_count = 0;

                if (*file_count < MAX_TMP_FILES) {
                    (*file_count)++;
                } else {
                    char *dest_path = (meta->mask & FAN_OPEN) ? OPENED_PATH : MODIFIED_PATH;

                    mergetmp(path, dest_path, *file_count);
                    *file_count = 1;
                }

                close(meta->fd);
            }
        } while (len > 0);
    }
}

/**
 * @brief pre-finish cleanup
 *  
 * prepares the process to finish,
 * it cleans the memory and saves all the allocated data
 */
static void clean_loop(void) {
    syslog(LOG_INFO, "Daemon has stopped.");
    mergetmp(TMP_OPENED_PATH, OPENED_PATH, count_opening_tmp);
    mergetmp(TMP_MODIFIED_PATH, MODIFIED_PATH, count_modifying_tmp);
    clean_tables();
    close(fan_fd);
}

/**
 * @brief sets up the signals the daemon needs
 *  
 * sets up signals that can end the process, such as:
 *  
 * - SIGTERM
 *  
 * - SIGKILL
 *  
 */
static void setup_signals(void) {
    struct sigaction sig_act;
    sig_act.sa_handler = handle_term;
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_flags = 0;

    sigaction(SIGINT, &sig_act, NULL);
    sigaction(SIGTERM, &sig_act, NULL);
}

/** 
 * @brief sets up the files needed by the daemon
 *  
 * creates every file needed for processing data
 *  
 * see macros:
 *  
 * - OPENED_PATH
 *  
 * - MODIFIED_PATH
 *  
 * - BLACKLIST_PATH
 *  
 * - TMP_OPENED_PATH
 *  
 * - TMP_MODIFIED PATH
 */
static void setup_files(void) {
    struct stat st;

    if (stat("/var/log/file-listener", &st) == -1) {
        mkdir("/var/log/file-listener", 0644);
    }
    
    if (stat(OPENED_PATH, &st) == -1) {
        creat(OPENED_PATH, 0644);
    }
    
    if (stat(MODIFIED_PATH, &st) == -1) {
        creat(MODIFIED_PATH, 0644);
    }
    
    if (stat(BLACKLIST_PATH, &st) == -1) {
        creat(BLACKLIST_PATH, 0644);
    }

    if (stat("/tmp/file-listener", &st) == -1) {
        mkdir("/tmp/file-listener", 0644);
    }
    
    if (stat("/tmp/file-listener/opfiles", &st) == -1) {
        mkdir("/tmp/file-listener/opfiles", 0644);
    }
    
    if (stat("/tmp/file-listener/modfiles", &st) == -1) {
        mkdir("/tmp/file-listener/modfiles", 0644);
    }
}

int main(void) {
    setup_signals();

    openlog("file_listener", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon has started.");

    setup_files();

    init_fanotify("/");

    loop();
    clean_loop();

    closelog();
    return EXIT_SUCCESS;
}
