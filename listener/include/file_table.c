#include <stdio.h>
#include "file_table.h"

struct _file* getitem(struct _file **table, const char* key) {
    struct _file *item = NULL;
    HASH_FIND_STR(*table, key, item);
    return item;
}

int additem(struct _file **table, const char *key, const long value) {
    struct _file *item = NULL;

    HASH_FIND_STR(*table, key, item);
    if (item == NULL) {
        item = (struct _file *)malloc(sizeof(struct _file));
        if (item == NULL) {
            perror("malloc");
            return EXIT_FAILURE;
        }

        strncpy(item->key, key, sizeof(item->key));
        item->value = value;
        HASH_ADD_STR(*table, key, item);
    } else {
        item->value = value;
    }

    return EXIT_SUCCESS;
}

void clear_table(struct _file **table) {
    struct _file *item, *tmp;

    HASH_ITER(hh, *table, item, tmp) {
        HASH_DEL(*table, item);
        free(item);
    }

    *table = NULL;
}
