/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/fview/blob/main/src/listener/file_listener.c
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

#define _GNU_SOURCE
#include <stdio.h> /* perror, snprintf, ssize_t, remove */
#include <stdlib.h> /* malloc, free, strtol, EXIT_SUCCESS, EXIT_FAILURE */
#include <unistd.h> /* readlink */
#include <sys/fanotify.h> /* fanotify_init, fanotify_mark, fanotify_event_metadata, all the macros starting with FAN */
#include <sys/stat.h> /* mkdir, stat */
#include <syslog.h> /* syslog, openlog, closelog, all the macros starting with LOG */
#include <time.h> /* time */
#include <fcntl.h> /* creat, O_RDONLY, O_LARGEFILE AT_FDCWD */
#include <string.h> /* strerror, strcmp, strncmp, strncpy */
#include <signal.h> /* sigaction, sigemptyset, sa_handler, SIGTERM, SIGKILL, SIGUSER1, SIGUSER2 */
#include <errno.h> /* errno */
#include <stdint.h> /* uint32_t, uint16_t, UINT32_MAX */
#include <poll.h> /* poll, pollfd, POLLIN */
#include "uthash.h" /* HASH_DEL, HASH_ITER */
#include "file_table.h" /* _file, additem, clean_table */
#include "strutils.h" /* splitstr */
#include "fileutils.h" /* readfile, savefile, PATH_LENGTH */

#define SAVE_PATH "/var/log/file-listener/file-events" /* log file path for storing in disk file events recorded by fanotify */
#define BLACKLIST_PATH "/var/log/file-listener/file-listener.blacklist" /* file path for the blacklist file */

#define TMP_FILE_PATH "/tmp/file-listener/%u.tmp" /* temporary log file for storing in disk file events recorded by fanotify */

#define MAX_TMP_FILES 500 /* max temporary log files that can be created */
#define MAX_TMP_SIZE 250 /* max items a temporary file can store before opening a new temporary file */

#define INTERVAL_SEC 15 /* timout for each time the process saves data */

#define INT_BUFF_SIZE 11

/**
 * struct that stores the count of items the blacklist currently holds
 */
struct blacklist_updater {
    char ***blk_entries; /** > entries of the blacklist */
    size_t count; /** > count of entries */
};

volatile sig_atomic_t running = 1; /* flag for the main loop */

char **blk_entries; /* list to store all the blacklist entries */

uint16_t file_count = 1; /* counter for the current amount of opening temporary files created */

/** 
 * @brief main loop of the process 
 *  
 * starts recording events using fanotify and poll
 *  
 * each time an event is registered, saves it in a table
 *  
 * when reached a certain amount of saves, loads all the data to a permanent file
 * @param file_table table that stores all the file events recorded
 * @param fan_fd file descriptor of fanotify
 */
static void loop(struct _file **file_table, int fan_fd);

/**
 * @brief pre-finish cleanup
 *  
 * prepares the process to finish,
 * it cleans the memory and saves all the allocated data
 * @param file_table table that stores all the file events recorded
 * @param fan_fd file descriptor of fanotify
 */
static void clean_loop(struct _file **file_table, int fan_fd);

/**
 * @brief signal handling
 *  
 * when a signal is recieved, it updates the value of variable running to 0.
 * this makes the function loop to stop after the next iteration.
 *  
 * @param sig number of the signal recieved.
 * 
 */
static void terminate(const int sig);

/**
 * @brief returns a file path
 *  
 * given a file descriptor fd, reads the link from '/proc/self/fd/...'
 * to return the path of the file descriptor
 *  
 * @param fd file descriptor
 * @param buff buffer that is going to store the real path
 * @param size length of the path
 * @return real file path of a file descriptor
 */
static int getfilepath(const int fd, char *buff, size_t size);

/**
 * clears the current content stored in memory by the blacklist
 */
static void clear_blacklist(void);

/**
 * @brief checks if a line its inside the blacklist
 * 
 * comparing line to the path searched,
 * if the path is equal to the line, it marks it as found
 * 
 * @param line the line that is going to be compared
 * @param arg struct that holds the entries of the blacklist and the count of the entries
 */
static void blacklist_handler(char *line, void *arg);

/**
 * updates the content of the blacklist stored in memory
 */
static int update_blacklist(void);

/* on signal recieved calls the method for updating the blacklist */
static void updateblk(const int sig); 

/**
 * @brief returns if a path its inside a blacklist
 *  
 * reads the content stored in memory by the blacklist_entry struct
 * returns whether or not a path is inside the blacklist
 *  
 * @param path path that is going to be checked if its inside the blacklist
 * @return 1 if found, 0 if not found
 */
static int path_in_blacklist(const char *path);

/**
 * @brief handles a file line read
 * 
 * when reached a line while reading a file, handles it
 * by splitting using ':' as a delimiter
 * 
 * then adds that splitted line into a table
 * 
 * @param line line that is going to be processed
 * @param arg table that is going to be modified
 */
static void loadtable_handler(char *line, void *arg);

/**
 * @brief loads a file for storing its data
 *  
 * loads a file and parses its content
 * each line parsed will be stored as a new item in the table
 * in the process, it modifies the items in the table with each item added
 * 
 * @param path path of the file that is going to be read
 * @param table file table address
 * @return 1 if successful, 0 if failed
 */
static int loadtable(const char* path, struct _file **table);

/**
 * @brief returns the content of a table
 * 
 * given a table, returns its content in a malloc'ed array
 * @param table table thats going to be read
 * @param out malloc'ed array that is going to be returned
 * @return amount of entries alloc'ed
 */
static size_t get_file_content(struct _file **table, char ***out);

/**
 * @brief saves a struct into disk
 *  
 * saves the current content of a table into a file
 * truncates the file and re-writes the new content into it
 *  
 * @param table the struct that is going to be saved into disk
 * @param save_path path of the file where the entries are going to be stored in disk
 * @return 1 if successful, 0 if failed
 * 
 */
static int savetable(struct _file **table, const char *save_path);

/**
 * @brief merge all temporary files created into one single table
 *  
 * using a pre-formatted path, formats it up to tmp_count
 * and reads its content, after loading everything, 
 * saves the table into a permanent file path
 *  
 * @param save_path path of the file where the entries are going to be stored in disk
 * @return 1 if successful, 0 if failed
 */
static int mergetmp(const char *save_path);

/**
 * @brief custom signal handling
 *  
 * handles SIGUSR1 and merges both tables into permanent space on disk
 * @param sig number of the signal recieved.
 */
void mergeall(const int sig);

/** 
 * @brief initalize fanotify in a specific path
 *  
 * given a path, tries to initialize fanotify on that path
 * with the flags FAN_OPEN and FAN_MODIFY
 * 
 * @param path path where fanotify is going to be setted up
 * @return file descriptor of fanotify
*/
static int init_fanotify(const char *path);

/**
 * @brief sets up the signals the daemon needs
 *  
 * sets up signals that can end the process, such as:
 *  
 * - SIGTERM
 *  
 * - SIGKILL
 *  
 *  sets up both SIGUSR1 and SIGUSR2 for handling events like:
 * 
 * - merging file content
 * - updating blacklist
 */
static void setup_signals(void);

/** 
 * @brief sets up the files needed by the daemon
 *  
 * creates every file needed for processing data
 *  
 * see macros:
 *  
 * - SAVE_PATH
 *  
 * - BLACKLIST_PATH
 *  
 * - TMP_FILE_PATH
 */
static void setup_files(void);

int main(void) {
    struct _file *file_table = NULL; /* stores file events in memory */
    blk_entries = NULL;

    int fan_fd; /* file descriptor of fanotify events */

    setup_signals();

    openlog("file_listener", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon has started.");

    setup_files();
    update_blacklist();

    fan_fd = init_fanotify("/");
    if (fan_fd < 0) {
        return EXIT_FAILURE;
    }
    
    loop(&file_table, fan_fd);
    clean_loop(&file_table, fan_fd);
    
    syslog(LOG_INFO, "Daemon has stopped.");
    closelog();

    return EXIT_SUCCESS;
}

static void loop(struct _file **file_table, int fan_fd) {
    uint16_t content_count = 0; /* counter for the items the current temporary file has stored */

    /* poll if fanotify recieves an event */
    struct pollfd fds = {
        .fd = fan_fd,
        .events = POLLIN
    };

    time_t last_save = time(NULL);

    while(running) {
        int ret = poll(&fds, 1, 1000);
        if (ret == -1) {
            perror("poll");
            continue;
        }

        time_t now = time(NULL);
        if (now - last_save >= INTERVAL_SEC) {
            char current_path[PATH_LENGTH];

            snprintf(current_path, sizeof(current_path), TMP_FILE_PATH, file_count);

            savetable(file_table, current_path);
            last_save = now;
        }

        if (ret < 0 || !(fds.revents & POLLIN))
            continue;

        struct fanotify_event_metadata buffer[200];
        ssize_t len;

        do {
            len = read(fan_fd, buffer, sizeof(buffer));
            if (len <= 0) break;

            if (len == -1 && errno != EAGAIN) {
                syslog(LOG_ERR, "Error: Couldnt read event metadata from file descriptior '%d'. -> %s", fan_fd, strerror(errno));
                break;
            }

            struct fanotify_event_metadata *meta;
            for (meta = buffer; FAN_EVENT_OK(meta, len); meta = FAN_EVENT_NEXT(meta, len)) {
                if (!(meta->mask & FAN_OPEN) && !(meta->mask & FAN_MODIFY)) {
                    close(meta->fd);
                    continue;
                }

                char *filepath = (char *)malloc(sizeof(char) * PATH_LENGTH);
                if (getfilepath(meta->fd, filepath, PATH_LENGTH) == -1) {
                    close(meta->fd);
                    continue;
                }

                if (path_in_blacklist(filepath)) {
                    close(meta->fd);
                    continue;
                }

                int added = (meta->mask & FAN_OPEN) ? additem(file_table, filepath, 1U, 0U) : additem(file_table, filepath, 0U, 1U);
                if (added && added != -1)
                    content_count++;

                free(filepath);
                if (content_count < MAX_TMP_SIZE) {
                    close(meta->fd);
                    continue;
                }

                char current_path[PATH_LENGTH];
                snprintf(current_path, sizeof(current_path), TMP_FILE_PATH, file_count);

                savetable(file_table, current_path);
                clear_table(file_table);

                content_count = 0;

                if (file_count < MAX_TMP_FILES) {
                    file_count++;
                } else {
                    mergetmp(SAVE_PATH);
                }

                close(meta->fd);
            }
        } while (len > 0);
    }
}

static void clean_loop(struct _file **file_table, int fan_fd) {
    mergetmp(SAVE_PATH);
    clear_table(file_table);
    clear_blacklist();
    close(fan_fd);
}

static void loadtable_handler(char *line, void *arg) {
    struct _file **table = (struct _file **)arg;

    char **splitted_line = NULL;
    size_t count = splitstr(line, ':', &splitted_line);
    if (splitted_line == NULL       || 
        splitted_line[0] == NULL    || 
        splitted_line[1] == NULL    ||
        splitted_line[2] == NULL)
        goto clean_splitted;

    if (count != 3) {
        goto clean_splitted;
    }

    char filename[PATH_LENGTH];
    char op_str[INT_BUFF_SIZE];
    char mod_str[INT_BUFF_SIZE];

    strncpy(filename, splitted_line[0], PATH_LENGTH - 1);
    filename[PATH_LENGTH - 1] = '\0';
    
    strncpy(op_str, splitted_line[1], INT_BUFF_SIZE - 1);
    op_str[INT_BUFF_SIZE - 1] = '\0';

    strncpy(mod_str, splitted_line[2], INT_BUFF_SIZE - 1);
    mod_str[INT_BUFF_SIZE - 1] = '\0';

    char *endptr;
    long tmp_op = strtol(op_str, &endptr, 10);
    long tmp_mod = strtol(mod_str, &endptr, 10);

    if (endptr == op_str || endptr == mod_str) {
        syslog(LOG_ERR, "Hmmm, are you modifying the files, arent you?. Non-numeric value encountered.\n");
        goto clean_splitted;
    }

    if (tmp_op > UINT32_MAX)
        tmp_op = UINT32_MAX;
    
    if (tmp_mod > UINT32_MAX)
        tmp_mod = UINT32_MAX;

    uint32_t op_count = (uint32_t)tmp_op;
    uint32_t mod_count = (uint32_t)tmp_mod;

    additem(table, filename, op_count, mod_count);

    clean_splitted:
        if (!splitted_line) return;

        for (size_t i = 0; splitted_line[i]; i++) {
            free(splitted_line[i]);
        }

        free(splitted_line);
}

static int loadtable(const char* path, struct _file **table) {
    return readfile(path, loadtable_handler, table);
}

static size_t get_file_content(struct _file **table, char ***out) {
    *out = (char **)malloc(sizeof(char *));
    if (!*out) {
        perror("malloc");
        *out = NULL;
        return 0;
    }

    size_t size = 0;

    struct _file *item, *tmp;
    HASH_ITER(hh, *table, item, tmp) {
        char *filename = item->key;

        struct stat st;
        if (stat(filename, &st) == -1) {
            HASH_DEL(*table, item);
            free(item);
            continue;
        }

        if (path_in_blacklist(filename)) {
            HASH_DEL(*table, item);
            free(item);
            continue;
        }

        uint32_t op_count = item->opening;
        uint32_t mod_count = item->modifying;

        char entry[PATH_LENGTH];
        snprintf(entry, sizeof(entry), "%s:%u:%u\n", filename, op_count, mod_count);

        char **tmp = (char **)realloc(*out, sizeof(char *) * (size + 2));
        if (!tmp) {
            perror("realloc");
        
            for (size_t i = 0; (*out)[i]; i++) {
                free((*out)[i]);
            }
            free(*out);

            *out = NULL;
            return 0;
        }

        tmp[size++] = strdup(entry);
        tmp[size] = NULL;
        *out = tmp;
    }

    return size;
}

static int savetable(struct _file **table, const char *path) {
    if (*table == NULL) {
        return 0;
    }

    char **content = NULL;
    get_file_content(table, &content);
    if (!content) {
        return 0;
    }

    int r = savefile(path, content, 1);

    for (size_t i = 0; content[i]; i++) {
        free(content[i]);
    }
    free(content);

    return r;
}

static int mergetmp(const char *save_path) {
    struct _file *merged_table = NULL;

    for (uint16_t i = 1; i <= file_count; i++) {
        char realpath[PATH_LENGTH];
        snprintf(realpath, sizeof(realpath), TMP_FILE_PATH, i);
        
        loadtable(realpath, &merged_table);
        remove(realpath);
    }

    file_count = 1;

    int r = savetable(&merged_table, save_path);
    clear_table(&merged_table);
    return r;
}

static void blacklist_handler(char *line, void *arg) {
    struct blacklist_updater *updater = (struct blacklist_updater *)arg;

    char **tmp = (char **)realloc(*(updater->blk_entries), sizeof(char *) * (updater->count + 2));
    if (!tmp) {
        perror("malloc");
        clear_blacklist();
        return;
    }

    tmp[updater->count] = strdup(line);
    tmp[++(updater->count)] = NULL;
    *(updater->blk_entries) = tmp;
}

static int update_blacklist(void) {
    clear_blacklist();

    blk_entries = (char **)calloc(1, sizeof(char *));
    if (!blk_entries) {
        perror("calloc");
        return 0;
    }

    struct blacklist_updater blk_updater = {
        .blk_entries = &blk_entries,
        .count = 0UL
    };

    return readfile(BLACKLIST_PATH, blacklist_handler, &blk_updater);
}

static void clear_blacklist(void) {
    if (!blk_entries) return;

    for (size_t i = 0; blk_entries[i]; i++) {
        free(blk_entries[i]);
    }
    free(blk_entries);

    blk_entries = NULL;
}

static int path_in_blacklist(const char *path) {
    /* paths that must be ignored regardless the blacklist file content */
    if (strcmp(path, SAVE_PATH) == 0                    || 
            strcmp(path, BLACKLIST_PATH) == 0           ||
            strncmp(path, TMP_FILE_PATH, 21) == 0       ||
            strncmp(path, "/proc/", 6) == 0             ||
            strncmp(path, "/dev/", 6) == 0              ||
            strncmp(path, "/sys/", 6) == 0              ||
            strncmp(path, "/run/", 6) == 0)
        return 1;

    for (size_t i = 0; blk_entries[i]; i++) {
        if (strncmp(blk_entries[i], path, strlen(blk_entries[i])) == 0) {
            return 1;
        }
    }

    return 0;
}

static void terminate(const int sig) {
    syslog(LOG_INFO, "Signal %s recieved, stopping process.", strsignal(sig));
    running = 0;
}

static void updateblk(const int sig) {
    syslog(LOG_INFO, "Signal %s recieved. Updating blacklist...", strsignal(sig));
    update_blacklist();
} 

void mergeall(const int sig) {
    syslog(LOG_INFO, "Signal %s recieved, merging content...", strsignal(sig));
    mergetmp(SAVE_PATH);
}

static int getfilepath(const int fd, char *buff, size_t size) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);

    ssize_t r = readlink(path, buff, size - 1);
    if (r == -1)
        return -1;

    buff[r] = '\0';
    return 0;
}

static int init_fanotify(const char *path) {
    int fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) {
        syslog(LOG_ERR, "Error: Couldnt initialize fanotify. -> %s", strerror(errno));
        return -1;
    }

    if (fanotify_mark(fan_fd,
                        FAN_MARK_ADD | FAN_MARK_MOUNT,
                        FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD,
                        AT_FDCWD,
                        path) == -1) {
        syslog(LOG_ERR, "Error: Couldnt mark mount point to fanotify in '%s' -> %s", path, strerror(errno));
        close(fan_fd);
        return -1;
    }

    return fan_fd;
}

static void setup_signals(void) {
    struct sigaction term_act;
    term_act.sa_handler = terminate;
    sigemptyset(&term_act.sa_mask);
    term_act.sa_flags = 0;

    struct sigaction merger_act;
    merger_act.sa_handler = mergeall;
    sigemptyset(&merger_act.sa_mask);
    merger_act.sa_flags = 0;

    struct sigaction updater_act;
    updater_act.sa_handler = updateblk;
    sigemptyset(&updater_act.sa_mask);
    updater_act.sa_flags = 0;
    
    sigaction(SIGINT, &term_act, NULL);
    sigaction(SIGTERM, &term_act, NULL);
    sigaction(SIGUSR1, &merger_act, NULL);
    sigaction(SIGUSR2, &updater_act, NULL);
}

static void setup_files(void) {
    struct stat st;

    if (stat("/var/log/file-listener", &st) == -1) {
        mkdir("/var/log/file-listener", 0744);
    }
    
    if (stat(SAVE_PATH, &st) == -1) {
        creat(SAVE_PATH, 0644);
    }

    if (stat(BLACKLIST_PATH, &st) == -1) {
        creat(BLACKLIST_PATH, 0644);
    }

    if (stat("/tmp/file-listener", &st) == -1) {
        mkdir("/tmp/file-listener", 0744);
    }
}
