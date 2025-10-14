/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/fview/tree/main/listener/include/fileutils.h
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

#ifndef _FILE_UTILS_H_
#define _FILE_UTILS_H_

#define _GNU_SOURCE
#include <unistd.h> /* open, read, write, close */
#include <fcntl.h> /* O_RDONLY, O_CREAT, O_TRUNC, O_WRONLY */

#define PATH_LENGTH 4096
#define BUFFER_SIZE 1024

static inline int readfile(const char *path, void (*line_handler)(char *line, void *arg), void *arg) {
    int fd = open(path, O_RDONLY | O_CREAT, 0644);
    if (fd == -1) {
        perror("open");
        return 0;
    }

    char line[PATH_LENGTH];
    char content[BUFFER_SIZE];
    ssize_t bytes_read;
    int line_pos = 0;

    while ((bytes_read = read(fd, content, sizeof(content))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (content[i] == '\n' || line_pos >= PATH_LENGTH - 1) {
                line[line_pos] = '\0';
                line_handler(line, arg);
                line_pos = 0;
            } else {
                line[line_pos++] = content[i];
            }
        }
    }

    if (line_pos > 0) {
        line[line_pos] = '\0';
        line_handler(line, arg);
    }

    close(fd);
    return 1;
}

static inline int savefile(const char *path, char **content, int trunc) {
    int flags = O_WRONLY | O_CREAT;
    if (trunc) flags |= O_TRUNC;

    int fd = open(path, flags, 0644);
    if (fd == -1) {
        perror("open");
        return 0;
    }

    for (size_t i = 0; content[i]; i++) {
        ssize_t bytes_written = write(fd, content[i], strlen(content[i]));
        if (bytes_written == -1) {
            perror("write");
            close(fd);
            return 0;
        }
    }

    close(fd);
    return 1;
}

static inline int appendline(const char *path, char *line, int trunc) {
    int flags = O_WRONLY | O_CREAT;
    if (trunc) flags |= O_TRUNC;

    int fd = open(path, flags, 0644);
    if (fd == -1) {
        perror("open");
        return 0;
    }

    ssize_t bytes_written = write(fd, line, strlen(line));
    if (bytes_written == -1) {
        perror("write");
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}

#endif