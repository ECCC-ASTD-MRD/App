//! \file

#ifndef _str_h
#define _str_h

#include <string.h>

#define MIN(x,y) ((x < y) ? x : y)
#define MAX(x,y) ((x > y) ? x : y)
#define strend(S) (S+strlen(S))

char * strpath(
    const char * const path,
    const char * const file
);
char * strcatalloc(
    char * dst,
    const char * const src
);
void strtrim(
    char * const str,
    const char pad
);
int strrindex(
    char * const str
);
int strtok_count(
    const char * const str,
    const char seperator
);
int strmatch(
    const char * const str,
    const char * const regex
);


//! Get the shortest of the actual string length or maximum length
static inline size_t strlen_up_to(
    //! [in] String for which to measure the length
    const char * const string,
    //! [in] Maximum length
    const size_t maxLength
) {
    return MIN(strlen(string), MAX(maxLength, 0));
}


//! Replace all occurrences of a given character with another one
static inline void strrep(
    //! [in,out] String in which to replace the characters
    char * Str,
    //! [in] Character to be replaced
    const char oldChar,
    //! [in] Replacement character
    const char newChar
) {
    if (Str) {
        while(*Str++ != '\0') {
            if (*Str == oldChar) *Str = newChar;
        }
    }
}


//! Pad the end of a string with spaces and terminate with NULL
static inline void strblank2end(
    //! [in,out] String to pad with spaces
    char * str,
    //! [in] String length
    int length
) {
    for (int i = strlen(str); i < length; i++) {
        str[i] = ' ';
    }
    str[length - 1] = '\0';
}


//! Fill a string with spaces and terminate with NULL
static inline void strblank_full(
    //! [out]
    char * str,
    int length
) {
    for (int i = 0; i < length - 1; i++) {
        str[i] = ' ';
    }
    str[length - 1] = '\0';
}

#endif