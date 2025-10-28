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

#define _GNU_SOURCE
#include <stdio.h> /* perror, snprintf, ssize_t, remove */
#include <stdlib.h> /* exit, EXIT_SUCCESS, EXIT_FAILURE, malloc, free, strtol */
#include <unistd.h> /* readlink */
#include <sys/fanotify.h> /* fanotify_init, fanotify_mark, fanotify_event_metadata, all the macros starting with FAN */
#include <sys/stat.h> /* mkdir, stat */
#include <syslog.h> /* syslog, openlog, closelog, all the macros starting with LOG */
#include <time.h> /* time */
#include <fcntl.h> /* creat, O_RDONLY, O_LARGEFILE AT_FDCWD */
#include <string.h> /* strerror, strcmp, strncmp */
#include <signal.h> /* sigaction, sigemptyset, sa_handler, SIGTERM, SIGKILL, SIGUSER1, SIGUSER2 */
#include <errno.h> /* errno */
#include <stdint.h> /* uint32_t, uint16_t, UINT32_MAX */
#include <poll.h> /* poll, pollfd, POLLIN */
#include "file_table.h" /* _file, additem, getitem, clean_table, HASH_ITER */
#include "strutils.h" /* splitstr */
#include "fileutils.h" /* readfile, savefile, PATH_LENGTH */

#define OPENED_PATH "/var/log/file-listener/opfiles" /* log file path for storing in disk opening events */
#define MODIFIED_PATH "/var/log/file-listener/modfiles" /* log file path for storing in disk modifying events */
#define BLACKLIST_PATH "/var/log/file-listener/file_listener.blacklist" /* file path for the blacklist file */

#define TMP_OPENED_PATH "/tmp/file-listener/opfiles/%u.tmp" /* temporary log file for storing in disk opening events */
#define TMP_MODIFIED_PATH "/tmp/file-listener/modfiles/%u.tmp" /* temporary log file for storing in disk modifying events */

#define MAX_TMP_FILES 500 /* max temporary log files that can be created */
#define MAX_TMP_SIZE 100 /* max items a temporary file can store before opening a new temporary file */

#define INTERVAL_SEC 5 /* timout for each time the process saves data */

/**
 * stores in memory a blacklist entry 
 * read from the blacklist file
 */
struct blacklist_entry {
    char path[PATH_LENGTH]; /** > path of the entry */
    UT_hash_handle hh; /** hashable */
};

/**
 * struct used by the loadtable function
 * so it knows which field to update by its event
 */
struct loader_element {
    struct _file **table; /** > table storing all files */
    const char *readingpath; /** > path of the file being loaded */
};

volatile sig_atomic_t running = 1; /* flag for the main loop */

struct _file *file_table = NULL; /* stores file events in memory */

uint16_t count_opening_tmp = 1; /* counter for the current amount of opening temporary files created */
uint16_t count_modifying_tmp = 1; /* counter for the current amount of modifying temporary files created */

uint16_t current_opening_size = 0; /* counter for the items the current opening temporary file has stored */
uint16_t current_modifying_size = 0; /* counter for the itmes the current modifying temporary file has stored */

int fan_fd; /* file descriptor of fanotify events */
struct pollfd fds[1]; /* poll if fanotify recieves an event */

struct blacklist_entry *blk_entries = NULL; /* table to store all the entries */

/** 
 * @brief main loop of the process 
 *  
 * starts recording events using fanotify and poll
 *  
 * each time an event is registered, saves it in a table
 *  
 * when reached a certain amount of saves, loads all the data to a permanent file
 * 
 */
static void loop(void);

/**
 * @brief pre-finish cleanup
 *  
 * prepares the process to finish,
 * it cleans the memory and saves all the allocated data
 */
static void clean_loop(void);

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
 * @param arg blacklist structure that stored both the path and found flag
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
 * @param loader struct to keep track of both the table and the current reading path
 * @return 1 if successful, 0 if failed
 */
static int loadtable(const char* path, struct loader_element *loader);

/**
 * @brief returns the content of a table
 * 
 * given a table, returns its content in a malloc'ed array
 * @param table table thats going to be read
 * @param out malloc'ed array that is going to be returned
 * @param event event that is going to indicate the function which field to save
 * @return amount of entries alloc'ed
 */
static size_t get_file_content(struct _file **table, char ***out, enum _event event);

/**
 * @brief saves a struct into disk
 *  
 * saves the current content of a table into a file
 * truncates the file and re-writes the new content into it
 *  
 * @param table the struct that is going to be saved into disk
 * @param op_path path of the file which stores opened events that is going to be trunced
 * @param mod_path path of the file which stores modified events that is going to be trunced
 * @return 1 if successful, 0 if failed
 * 
 */
static int savetable(struct _file **table, const char *op_path, const char *mod_path);

/**
 * @brief merge all temporary files created into one single table
 *  
 * using a pre-formatted path, formats it up to tmp_count
 * and reads its content, after loading everything, 
 * saves the table into a permanent file path
 *  
 * @param path temporary file path
 * @param op_path path where all the temporary opened files are going to be stored
 * @param mod_path path where all the temporary modified files are going to be stored
 * @param tmp_count count of how many temporary files are there
 * @return exit code (0, 1)
 */
static int mergetmp(const char *path, const char *op_path, const char *mod_path, uint16_t *tmp_count);

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
*/
static void init_fanotify(const char *path);

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
static void setup_files(void);

int main(void) {
    setup_signals();

    openlog("file_listener", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon has started.");

    setup_files();
    update_blacklist();

    init_fanotify("/");

    loop();
    clean_loop();

    closelog();
    return EXIT_SUCCESS;
}

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

            savetable(&file_table, current_oppath, current_modpath);
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

                char *filepath = (char *)malloc(sizeof(char) * PATH_LENGTH);
                if (getfilepath(meta->fd, filepath, PATH_LENGTH) == -1) {
                    close(meta->fd);
                    continue;
                }
                

                if (path_in_blacklist(filepath)) {
                    close(meta->fd);
                    continue;
                }

                enum _event event = (meta->mask & FAN_OPEN) ? F_OPENED : F_MODIFIED;

                uint16_t *content_count = (meta->mask & FAN_OPEN) ? &current_opening_size : &current_modifying_size;

                struct _file *file = NULL;
                getitem(&file_table, filepath, &file);
                if (!file) {
                    additem(&file_table, filepath, 1U, event);
                    (*content_count)++;
                } else {
                    if (event == F_OPENED) {
                        file->opening += 1U;
                    } else {
                        file->modifying += 1U;
                    }
                }

                free(filepath);

                if (*content_count < MAX_TMP_SIZE) {
                    close(meta->fd);
                    continue;
                }

                uint16_t *file_count = (meta->mask & FAN_OPEN) ? &count_opening_tmp : &count_modifying_tmp;
                char *path = (meta->mask & FAN_OPEN) ? TMP_OPENED_PATH : TMP_MODIFIED_PATH;

                char current_oppath[PATH_LENGTH];
                char current_modpath[PATH_LENGTH];

                snprintf(current_oppath, sizeof(current_oppath), TMP_OPENED_PATH, count_opening_tmp);
                snprintf(current_modpath, sizeof(current_modpath), TMP_MODIFIED_PATH, count_modifying_tmp);

                savetable(&file_table, current_oppath, current_modpath);
                clear_table(&file_table);

                *content_count = 0;

                if (*file_count < MAX_TMP_FILES) {
                    (*file_count)++;
                } else {
                    mergetmp(path, OPENED_PATH, MODIFIED_PATH, file_count);
                }

                close(meta->fd);
            }
        } while (len > 0);
    }
}

static void clean_loop(void) {
    syslog(LOG_INFO, "Daemon has stopped.");
    mergetmp(TMP_OPENED_PATH, OPENED_PATH, MODIFIED_PATH, &count_opening_tmp);
    mergetmp(TMP_MODIFIED_PATH, OPENED_PATH, MODIFIED_PATH, &count_modifying_tmp);
    clear_table(&file_table);
    close(fan_fd);
}

static void loadtable_handler(char *line, void *arg) {
    struct loader_element *loader = (struct loader_element *)arg;

    char **splitted_line = NULL;
    size_t count = splitstr(line, ':', &splitted_line);
    if (splitted_line == NULL || splitted_line[0] == NULL || splitted_line[1] == NULL) {
        return;
    }

    if (count != 2) {
        for (size_t i = 0; splitted_line[i] != NULL; i++) {
            free(splitted_line[i]);
        }

        free(splitted_line);
        return;
    }

    char key[PATH_LENGTH];
    char value_str[PATH_LENGTH];
    strcpy(key, splitted_line[0]);
    strcpy(value_str, splitted_line[1]);

    char *endptr;
    long tmp = strtol(value_str, &endptr, 10);
    if (endptr == value_str) {
        for (size_t i = 0; splitted_line[i] != NULL; i++) {
            free(splitted_line[i]);
        }

        free(splitted_line);

        syslog(LOG_ERR, "Hmmm, are you modifying the files, arent you?. Non-numeric value encountered.\n");
        return;
    }

    if (tmp > UINT32_MAX)
        tmp = UINT32_MAX;

    uint32_t value = (uint32_t)tmp;

    int is_opening = strncmp(loader->readingpath, TMP_OPENED_PATH, 21) == 0;
    enum _event event = (is_opening) ? F_OPENED : F_MODIFIED;

    struct _file *file = NULL;
    getitem(loader->table, key, &file);
    if (!file) {
        additem(loader->table, key, value, event);
    } else {
        if (event == F_OPENED) {
            file->opening += value;
        } else {
            file->modifying += value;
        }
    }

    for (size_t i = 0; splitted_line[i] != NULL; i++) {
        free(splitted_line[i]);
    }

    free(splitted_line);
}

static int loadtable(const char* path, struct loader_element *loader) {
    return readfile(path, loadtable_handler, loader);
}

static size_t get_file_content(struct _file **table, char ***out, enum _event event) {
    *out = (char **)malloc(sizeof(char *));
    if (!*out) {
        perror("malloc");
        return 0;
    }

    size_t size = 0;

    struct _file *item, *tmp;
    HASH_ITER(hh, *table, item, tmp) {
        char *filename = item->key;

        struct stat st;
        if (stat(filename, &st) == -1) {
            HASH_DEL(*table, item);
            continue;
        }

        if (path_in_blacklist(filename)) {
            HASH_DEL(*table, item);
            continue;
        }

        uint32_t count;
        if (event == F_OPENED) {
            count = item->opening;
        } else {
            count = item->modifying;
        }

        if (count <= 0)
            continue;
        
        char entry[PATH_LENGTH];
        snprintf(entry, sizeof(entry), "%s:%u\n", filename, count);
        
        char **tmp = (char **)realloc(*out, sizeof(char *) * (size + 1));
        if (!tmp) {
            perror("realloc");
            return 0;
        }
        
        tmp[size++] = strdup(entry);
        *out = tmp;
    }

    char **final = (char **)realloc(*out, sizeof(char *) * (size + 1));
    if (!final) {
        perror("realloc");
        for (size_t i = 0; *out[i] != NULL; i++) {
            free(*out[i]);
        }

        free(*out);
        return 0;
    }

    final[size] = NULL;
    *out = final;
    return size;
}

static int savetable(struct _file **table, const char *op_path, const char *mod_path) {
    if (*table == NULL) {
        return 0;
    }

    char **op_content = NULL;
    get_file_content(table, &op_content, F_OPENED);
    if (!op_content) {
        goto clean_content;
    }

    char **mod_content = NULL;
    get_file_content(table, &mod_content, F_MODIFIED);
    if (!mod_content) {
        goto clean_content;
    }

    int r_op = savefile(op_path, op_content, 1);
    int r_mod = savefile(mod_path, mod_content, 1);

    clean_content:
        for (size_t i = 0; op_content[i]; i++) {
            free(op_content[i]);
        }
        free(op_content);

        for (size_t i = 0; mod_content[i]; i++) {
            free(mod_content[i]);
        }
        free(mod_content);

        if (r_op != -1 && r_mod != -1)
            return r_op && r_mod;
        return 0;
}

static int mergetmp(const char *path, const char *op_path, const char *mod_path, uint16_t *tmp_count) {
    struct _file *merged_table = NULL;

    struct loader_element loader = {
        .table = &merged_table,
        .readingpath = path
    };

    for (uint16_t i = 1; i <= *tmp_count; i++) {
        char realpath[PATH_LENGTH];
        snprintf(realpath, sizeof(realpath), path, i);
        
        loadtable(realpath, &loader);
        remove(realpath);
    }

    *tmp_count = 1;

    int r = savetable(&merged_table, op_path, mod_path);
    clear_table(&merged_table);
    return r;
}

void mergeall(const int sig) {
    syslog(LOG_INFO, "Signal %s recieved, merging content...", strsignal(sig));

    mergetmp(TMP_OPENED_PATH, OPENED_PATH, MODIFIED_PATH, &count_opening_tmp);
    mergetmp(TMP_MODIFIED_PATH, OPENED_PATH, MODIFIED_PATH, &count_modifying_tmp);
}

static void clear_blacklist(void) {
    struct blacklist_entry *item, *tmp;
    HASH_ITER(hh, blk_entries, item, tmp) {
        HASH_DEL(blk_entries, item);
        free(item);
    }

    blk_entries = NULL;
}

static void blacklist_handler(char *line, void *arg) {
    struct blacklist_entry **blk_entries = (struct blacklist_entry **)arg;

    struct blacklist_entry *item = (struct blacklist_entry *)malloc(sizeof(struct blacklist_entry));
    if (!item) {
        perror("malloc");
        clear_blacklist();
        return;
    }

    char path[PATH_LENGTH];
    strcpy(item->path, line);
    strcpy(path, line);

    HASH_ADD_STR(*blk_entries, path, item);
}

static int update_blacklist(void) {
    clear_blacklist();
    return readfile(BLACKLIST_PATH, blacklist_handler, &blk_entries);
}

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

    int found = 0;
    struct blacklist_entry *item, *tmp;
    HASH_ITER(hh, blk_entries, item, tmp) {
        if (strncmp(item->path, path, strlen(item->path)) == 0) {
            found = 1;
            break;
        }
    }
    
    return found;
}

static void updateblk(const int sig) {
    syslog(LOG_INFO, "Signal %s recieved. Updating blacklist...", strsignal(sig));
    update_blacklist();
} 

static void terminate(const int sig) {
    syslog(LOG_INFO, "Signal %s recieved, stopping process.", strsignal(sig));
    running = 0;
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

static void init_fanotify(const char *path) {
    fan_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) {
        syslog(LOG_ERR, "Error: Couldnt initialize fanotify. -> %s", strerror(errno));
        clean_loop();
        exit(EXIT_FAILURE);
    }

    if (fanotify_mark(fan_fd,
                        FAN_MARK_ADD | FAN_MARK_MOUNT,
                        FAN_OPEN | FAN_MODIFY | FAN_EVENT_ON_CHILD,
                        AT_FDCWD,
                        path) == -1) {
        syslog(LOG_ERR, "Error: Couldnt mark mount point to fanotify in '%s' -> %s", path, strerror(errno));
        clean_loop();
        exit(EXIT_FAILURE);
    }

    fds[0].fd = fan_fd;
    fds[0].events = POLLIN;
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
