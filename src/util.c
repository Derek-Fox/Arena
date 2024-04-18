/* Module for "utility functions" - general functions that provide
 * useful functionality, but are not tied to a particular data type or
 * module.
 */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "util.h"

/************************************************************************
 * "trim" is like the Java "trim" String method (or the "strip"
 * function in Python): It removes all whitespace at the beginning and
 * at the end of a string. It does this in place, by writing the NUL
 * byte at the appropriate ending position, and returning a pointer to
 * the beginning of the string with whitespace removed. Since it
 * modifies the string, it must be called with a writable buffer
 * (i.e., not with a string literal).
 */
char* trim(char* line) {
    int llen = strlen(line);
    if (llen > 0) {
        char* final = &line[llen - 1];
        while ((final >= line) && isspace(*final))
            final--;
        *(final + 1) = '\0';
    }

    while (isspace(*line))
        line++;

    return line;
}

/************************************************************************
 * Checks if a string is alphanumeric.
 */
int strisalnum(char* str) {
    for (size_t i = 0; i < strlen(str); i++) {
        if (!isalnum(str[i])) {
            return 0;
        }
    }
    return 1;
}


