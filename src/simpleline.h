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
    SL_OK = 0,
    SL_EXCEPTION_OUT_OF_MEMORY,
    SL_EXCEPTION_EOF
} sl_exception_t;

typedef struct {
    char* buf;
    sl_ull len;
    sl_ull capacity;
    sl_exception_t exception;
} sl_result_t;

// Function Signatures
sl_result_t sl_input(char* prompt);

static sl_result_t sl_non_interactive_readline(char* prompt);
    // Fallback When IO Is Not Interactive

// Free result buffer and zero out the struct
void sl_free_result(sl_result_t* result) {
    if (result && result->buf) {
        free(result->buf);
        result->buf = NULL;
        result->len = 0;
        result->capacity = 0;
    }
}

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
sl_result_t sl_input(char* prompt) {
    return sl_non_interactive_readline(prompt);
}
#else
sl_result_t sl_input(char* prompt) {
    return (sl_result_t){NULL, 0, 0, SL_OK};
}
#endif

static sl_ull sl_readline_buffer_init_size = 128;
static sl_result_t sl_non_interactive_readline(char* prompt) {
    sl_ull cap = sl_readline_buffer_init_size;
    sl_ull l = 0;

    // Display Prompt
    fputs(prompt,stdout);
    fflush(stdout);

    // Read Input
    char* buf = malloc(cap);
    if (buf == NULL)
        return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_OUT_OF_MEMORY};

    // UTF-8 Is Already Supported For UTF-8 Leading Digits Restriction
    int c = getchar();
    while (c != EOF) {
        if (c == '\n') break;
        buf[l++] = c;
        if (l + 1 >= cap) {
            char* next_buf = realloc(buf,cap * 2);
            if (next_buf == NULL) {
                free(buf);
                return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_OUT_OF_MEMORY};
            }
            buf = next_buf;
            cap *= 2;
        }
        c = getchar();
    }

    // EOF with no input is end of stream; EOF after input
    // is just the last line (no trailing newline)
    if (c == EOF && l == 0) {
        free(buf);
        return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_EOF};
    }

    // Set Back Values
    buf[l] = 0;
    return (sl_result_t){buf, l, cap, SL_OK};
}

#endif