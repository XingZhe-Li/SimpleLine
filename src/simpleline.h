// Header Only Defines
#ifndef SIMPLELINE_H
#define SIMPLELINE_H

// Header files
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// System Related Headers
#ifndef _WIN32
#include <termios.h>
#endif

// Windows Does Not Need Such A Readline !!!
// The Feature Is Already Built In Conhost.exe !!!

// Types
typedef long long sl_ll;
typedef unsigned long long sl_ull;

typedef enum {
    SL_EXCEPTION_OUT_OF_MEMORY
} sl_exception_t;

// Function Signatures
char* sl_input(char*,sl_ull*,sl_ull*); 
    // Parameter: prompt, len, capacity

static char* sl_non_interactive_readline(char*,sl_ull*,sl_ull*);
    // Fallback When IO Is Not Interactive

// Global Variables
void (*sl_exception_hook)(sl_exception_t) = NULL;

// Utility Function Implementation
static sl_ll sl_unicode_len(char c) {
    if ((c & 0x80) == 0x00) {
        return 1;   // Single Bytes Character In UTF-8
    } else if ((c & 0xE0) == 0xC0) {
        return 2;   // 2 Bytes Character In UTF-8
    } else if ((c & 0xF0) == 0xE0) {
        return 3;   // 3 Bytes Character In UTF-8
    } else if ((c & 0xF8) == 0xF0) {
        return 4;   // 4 Bytes Character In UTF-8
    } else if ((c & 0xC0) == 0x80) {
        return 0;   // Body Character In UTF-8
    } else {
        return -1;  // Unknown Character Length
    }
}

// Core Function Implementation
#ifdef _WIN32
char *sl_input(char* prompt,sl_ull* len,sl_ull* capacity) {
    return sl_non_interactive_readline(prompt,len,capacity);
}
#else
char *sl_input(char* prompt,sl_ull* len,sl_ull* capacity) {
    return NULL;
}
#endif

static sl_ull sl_readline_buffer_init_size = 128;
static char* sl_non_interactive_readline(char* prompt,sl_ull* len,sl_ull* capacity) {
    sl_ull cap = sl_readline_buffer_init_size; 
    sl_ull l = 0;

    // Display Prompt
    fputs(prompt,stdout);
    fflush(stdout);

    // Read Input
    char* buf = malloc(cap); 
    if (buf == NULL) {
        if (sl_exception_hook)
            sl_exception_hook(SL_EXCEPTION_OUT_OF_MEMORY);
        if (capacity) *capacity = 0;
        if (len) *len = 0;
        return NULL;
    }

    // UTF-8 Is Already Supported For UTF-8 Leading Digits Restriction
    int c = getchar();
    while (c != EOF) {
        if (c == '\n') break;
        buf[l++] = c;
        if (l + 1 >= cap) {
            char* next_buf = realloc(buf,cap * 2);
            if (next_buf == NULL) {
                // Failed To Scale
                free(buf);
                if (sl_exception_hook)
                    sl_exception_hook(SL_EXCEPTION_OUT_OF_MEMORY);
                if (len) *len = 0;
                if (capacity) *capacity = 0;
                return NULL;
            }
            // Successfully Scaled
            buf = next_buf;
            cap *= 2;
        }
        c = getchar();
    }

    // Return Null If Met EOF
    if (c == EOF) {
        free(buf);
        if (capacity) *capacity = 0;
        if (len) *len = 0;
        return NULL;
    }

    // Set Back Values
    buf[l] = 0;
    if (capacity) *capacity = cap;
    if (len) *len = l;
    return buf;
}

#endif