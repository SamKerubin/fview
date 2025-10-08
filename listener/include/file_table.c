/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/fview/tree/main/listener/include/file_table.c
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

#include <stdio.h>
#include "file_table.h"
/** 
 * @brief given a key, returns an item _file inside a table 
 *  
 * searchs for an item stored inside a table that maches a key
 * returns that item if found
 *  
 * @param table table struct where the item is going to be searched
 * @param key key of the item being searched
 * @return returns a copy of the item if found, if not, returns NULL
 */
struct _file* getitem(struct _file **table, const char* key) {
    struct _file *item = NULL;
    HASH_FIND_STR(*table, key, item);
    return item;
}

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
 * @return exit code (0, 1)
 */
int additem(struct _file **table, const char *key, const long value) {
    struct _file *item = NULL;

    HASH_FIND_STR(*table, key, item);
    if (item == NULL) {
        item = (struct _file *)malloc(sizeof(struct _file));
        if (item == NULL) {
            perror("malloc");
            return EXIT_FAILURE;
        }

        strcpy(item->key, key);
        item->value = value;
        HASH_ADD_STR(*table, key, item);
    } else {
        long diff = abs(value - item->value);
        item->value += diff;
    }

    return EXIT_SUCCESS;
}

/** 
 * @brief frees all the values stored in a table
 *  
 * given a table, frees all the memory alloc'ed inside of it
 * @param table table struct thats going to be freed
 */
void clear_table(struct _file **table) {
    struct _file *item, *tmp;

    HASH_ITER(hh, *table, item, tmp) {
        HASH_DEL(*table, item);
        free(item);
    }

    *table = NULL;
}
