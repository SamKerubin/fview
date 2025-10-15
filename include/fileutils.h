/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/CLibs/tree/main/include/fileutils.h
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

#define PATH_LENGTH 4096
#define BUFFER_SIZE 1024

/**
 * @brief reads a file and processes its lines
 * 
 * @param path path of the file
 * @param line_handler handler in charge of processing lines
 * @param arg extra argument for the handler
 * @return 1 if successful, 0 if failed
 */
int readfile(const char *path, void (*line_handler)(char *line, void *arg), void *arg);

/**
 * @brief saves content into a file
 * 
 * @param path path of the file
 * @param content content that is going to be saved
 * @param trunc flag indicating if the file is going to be turnced
 * @return 1 if successful, 0 if failed
 */
int savefile(const char *path, char **content, int trunc);

/**
 * @brief saves a single line into a file
 * 
 * @param path path of the file
 * @param line line thats going to be stored
 * @param trunc flag indicating if the file is going to be trunced
 * @return 1 if successful, 0 if failed
 */
int appendline(const char *path, char *line, int trunc);

#endif /* _FILE_UTILS_H_ */