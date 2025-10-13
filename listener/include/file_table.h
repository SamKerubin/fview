/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/fview/tree/main/listener/include/file_table.h
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

#ifndef _FILE_TABLE_H_
#define _FILE_TABLE_H_

#include "uthash.h" /* UT_hash_handle, HASH_FIND_STR, HASH_ADD_STR, HASH_ITER */

/**
 * @brief struct that stores general information about a file
 *  
 * stores the name of the file and a counter of how many times
 * an event (open, modify) has occured
 *  
 * it also uses UT_hash_handle struct for handling the hash table behavior
 */
struct _file {
    char key[4096]; /** > file name */
    uint32_t value; /** > count of the event */
    UT_hash_handle hh; /** hashable */
};

struct _file* getitem(struct _file **table, const char *key);
int additem(struct _file **table, const char* key, const uint32_t value);
void clear_table(struct _file **table);

#endif /* _FILE_TABLE_H_ */
