/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/fview/blob/main/src/file_table.c
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

#include <stdio.h> /* perror */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* strncpy */
#include "file_table.h"

int additem(struct _file **table, const char *filename, const uint32_t op_count, const uint32_t mod_count) {
    if (!filename || *filename == '\0') {
        return -1;
    }

    struct _file *item = NULL;
    HASH_FIND_STR(*table, filename, item);
    if (!item) {
        item = (struct _file *)malloc(sizeof(struct _file));
        if (item == NULL) {
            perror("malloc");
            return -1;
        }

        strncpy(item->key, filename, sizeof(item->key) - 1);
        item->key[sizeof(item->key) - 1] = '\0';
        item->opening = op_count;
        item->modifying = mod_count;
        HASH_ADD_STR(*table, key, item);
        return 1;
    }

    item->opening += op_count;
    item->modifying += mod_count;

    return 0;
}

void clear_table(struct _file **table) {
    struct _file *item, *tmp;

    HASH_ITER(hh, *table, item, tmp) {
        HASH_DEL(*table, item);
        free(item);
    }
    *table = NULL;
}
