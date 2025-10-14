/*
Copyright (c) 2025, Sam  https://github.com/SamKerubin/fview/tree/main/listener/include/strutils.h
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

#ifndef _STRUTILS_H_
#define _STRUTILS_H_

#include <stdlib.h> /* malloc */
#include <string.h> /* strlen, strdup, strtok */

/**
 * @brief splits a string using a delimiter
 *  
 * given a string, counts how many times a delimiter appears and
 * splits the string using strtok
 *  
 * @param str the string that is going to be splitted
 * @param delimiter character that acts as the splitter
 * @param out alloc'ed array of the splitted tokens of the string
 * @return count of tokens the splitted string has
 */
static inline size_t splitstr(char *str, const char delimiter, char ***out) {
    if (!str || !delimiter) {
        *out = NULL;
        return 0;
    }

    char **result;
    char *tmp = str;
    char *last_delim = NULL;
    char delim[2];
    delim[0] = delimiter;
    delim[1] = '\0';      
    size_t count = 0;

    while (*tmp) {
        if (delimiter == *tmp) {     
            count++;    
            last_delim = tmp;          
        }         
        tmp++;    
    }

    if (last_delim != NULL && last_delim < (str + strlen(str) - 1))
        count++;  
    else if (last_delim == NULL)       
        count = 1;

    count += last_delim < (str + strlen(str) - 1);  
    count++;

    result = (char **)malloc((sizeof(char *) * count) + 1);          
    if (result == NULL) {
        *out = NULL;
        return 0;
    }

    size_t ind = 0;     
    char *token = strtok(str, delim); 
    while (token != NULL) {            
        result[ind++] = strdup(token);
        token = strtok(0, delim);
    }

    result[ind] = NULL; 
    *out = result;

    return ind;
}

#endif