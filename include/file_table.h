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

#include <stdint.h> /* uint32_t */
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
    uint32_t opening; /** > count of the opening event */
    uint32_t modifying; /** > count of the modifying event */
    UT_hash_handle hh; /** hashable */
};

/**
 * enum to describe which event has occured
 */
enum _event {
    F_OPENED, /** > when a file is opened */
    F_MODIFIED /** > when a file is modified */
};

/**
 *  @brief given a key and a value, adds it to a table
 *  
 * adds a value to a given table struct, if key is already on the table
 * updates it to value
 *  
 * if its not in the table already, adds it normally
 *  
 * @param table table struct where the item is going to be added
 * @param key key of the item being added (_file->key)
 * @param value value of the item being added (_file->value)
 * @param event type of the event (modified, openened)
 * @return exit code (0, 1)
 */
int additem(struct _file **table, const char *filename, const uint32_t value, enum _event event);

/** 
 * @brief frees all the values stored in a table
 *  
 * given a table, frees all the memory alloc'ed inside of it
 * @param table table struct thats going to be freed
 */
void clear_table(struct _file **table);

#endif /* _FILE_TABLE_H_ */
