#ifndef _FILE_TABLE_H_
#define _FILE_TABLE_H_

#include "uthash.h" /* UT_hash_handle, HASH_FIND_STR, HASH_ADD_STR, HASH_ITER */

#define PATH_LENGTH 4096 /* buffer limit for paths */

struct _file {
    char key[PATH_LENGTH]; /* file name */
    long value; /* count of the event */
    UT_hash_handle hh;
};

struct _file* getitem(struct _file **table, const char *key);
int additem(struct _file **table, const char* key, const long value);
void clear_table(struct _file **table);

#endif /* _FILE_TABLE_H_ */
