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
    size_t string_len = 0;
    while (string_len < maxLength && string[string_len] != '\0') { string_len++; }
    return string_len;
}

//! Copy at most `num_char` characters of string `src` into `dest` buffer, and append the NULL character if dest buffer
//! has enough space (num_char < dest_size).
//! WHen calling C-implemented functions from Fortran, there might not be an extra byte in `dest` to put a terminating
//! NULL character.
static inline void strncpy_safe(
    char* const dest,       //!< [out] Where we want to copy the input string
    const size_t dest_size, //!< [in]  Size of the destination buffer
    const char* const src,  //!< [in]  The string we want to copy
    const size_t num_char   //!< [in]  Maximum number of characters of the input string we want to copy
) {
    const size_t src_len = strlen_up_to(src, MIN(num_char, dest_size - 1));
    memcpy(dest, src, src_len);
    if (src_len < dest_size) dest[src_len] = '\0';
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
