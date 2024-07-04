#ifndef _str_h
#define _str_h

#include <string.h>

#define MIN(x,y) ((x < y) ? x : y)
#define MAX(x,y) ((x > y) ? x : y)
#define strend(S) (S+strlen(S))

char* strpath(char *Path,char *File);
char* strcatalloc(char *StrTo,char *StrFrom);
void  strtrim(char* Str,char Tok);
int   strtok_count(char *Str,char Sep);
int   strrindex(char *Str);
int   strmatch(const char *Str,char *Pattern);

static inline size_t strlen_up_to(const char* string, const size_t max_length) {
    return MIN(strlen(string),MAX(max_length, 0));
}

static inline void strrep(char *Str,char Tok,char Rep) {

   if (Str) {
      while(*Str++!='\0')
      if (*Str==Tok)
         *Str=Rep;
   }
}

static inline void strblank2end(char *Str,int Length) {
   int i;

   for (i=strlen(Str);i<Length;i++) {
      Str[i]=' ';
   }
   Str[Length-1] = '\0';
}

static inline void strblank_full(char *Str, int Length) {
   for (int i = 0; i < Length - 1; i++) {
      Str[i] = ' ';
   }
   Str[Length - 1] = '\0';
}

#endif