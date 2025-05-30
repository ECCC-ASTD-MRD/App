//! \file
//! A collection of miscellaneous string functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <regex.h>
#include <str.h>


//! Get a new file path with the provided file name at the end
char * strpath(
    //! [in] Path
    const char * const path,
    //! [in] File name to append at the end
    const char * const file
) {
    //! \return New file path. Caller must call free() when lo longer needed
    const char * lastSlash = strrchr(path, '/');
    if (!lastSlash) lastSlash = path;
    char * const new = (char*)calloc(strlen(file) + (lastSlash - path) + 2, 1);

    strncpy(new, path, (lastSlash - path));
    strcat(new, "/");
    strcat(new, file);

    return new;
}


//! Append src to dst allocating as necessary
char * strcatalloc(
    //! [in,out] Destination string allocated or reallocated as necessary
    char * dst,
    //! [in] String to concatenate to dst
    const char * const src
) {
    //! \return Concatenated string (dst src)
    //! \note Caller must free dst when no longer needed.

    if (src) {
        if (dst) {
            dst = (char*)realloc(dst, strlen(dst) + strlen(src) + 1);
            strcat(dst, src);
        } else {
            dst = strdup(src);
        }
    }
    return dst;
}


//! Left justify the content of str which is not pad characters and replace remaining pad characters with NULL
void strtrim(
    //! [in,out] String on which to operate
    char * const str,
    //! [in] Padding character
    const char pad
) {
    if (str) {
        if (str[0] != '\0') {
            // Clear fisrt blanks
            register int nbPad = 0;
            while(*(str + nbPad) == pad) nbPad++;

            // Copy chars, including \0, toward beginning to remove pad characters
            char * currentChar = str;
            if (nbPad) {
                while(currentChar < str + strlen(str) - nbPad + 1) {
                    *currentChar = *(currentChar + nbPad);
                    currentChar++;
                }
            }

            // Clear end blanks
            currentChar = str + strlen(str);
            while(*--currentChar == pad) {
                *currentChar = '\0';
            }
        }
    }
}


//! Get the integer number between parentheses and remove it (includind the parentheses) from the input string
//! \todo Change the name of this function for something more appropriate and descriptive
int strrindex(
    //! [in,out] String on which to operate
    char * const str
) {
    //! \return Integer corresponding to the number between parentheses in the string, or -1 if not found
    int num = -1;
    if (str) {
        char * origStr = strdup(str);
        char * lParenPos = index(origStr, '(');
        char * rParenPos = index(origStr, ')');

        if (!lParenPos || !rParenPos) {
            free(origStr);
            return -1;
        }

        sscanf(lParenPos, "(%i)", &num);

        char * charItr = origStr;
        int newLen = 0;
        int copy = 1;
        while(*charItr != '\0') {
            if (*charItr == '(') {
                copy = 0;
            }

            if (copy) str[newLen++] = *charItr;

            if (*charItr == ')') {
                copy = 1;
            }
            charItr++;
        }
        str[newLen] = '\0';
        free(origStr);
    }
    return num;
}


//! Get the number of tokens in a string
//! \bug The number of tokens makes no sense depending on the where to seperator occurs into string
//! | Test string  | Count |
//! | ------------ | ----- |
//! | ""           |     0 |
//! | ","          |     1 |
//! | "abc,def"    |     2 |
//! | ",abc,def"   |     2 |
//! | ",abc,def,"  |     3 |
//! | ",abc,,def," |     3 |
int strtok_count(
    //! [in] String in which to count the tokens
    const char * const str,
    //! [in] Token seperator
    const char seperator
) {
    int n = 0;
    int s = 1;

    const char * charItr = str;
    while(*charItr++ != '\0') {
        if (*charItr == '\n') break;

        if (*charItr != seperator) {
            if (s) {
                s = 0;
                n++;
            }
        } else {
            s = 1;
        }
    }
    return n;
}


//! Test if a string matches with a regex
int strmatch(
    //! [in] String on which to apply the regex
    const char * const str,
    //! [in] Regex
    const char * const regex
) {
    //! \returns 0 if the string matches the regex, non-zero otherwise
    int status = 1;
    regex_t re;
    if (regcomp(&re, regex, REG_EXTENDED|REG_NOSUB|REG_ICASE) == 0)  {
        status = regexec(&re, str, (size_t)0, NULL, 0);
        regfree(&re);
    }

    return status;
}
