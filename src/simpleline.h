// Header Only Defines
#ifndef SIMPLELINE_H
#define SIMPLELINE_H

// Header files
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// System Related Headers (POSIX-only)
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
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
void sl_free_result(sl_result_t* result);

// Free all history entries
void sl_free_history(void);

// Load history from a file (one line per entry)
// Returns true on success, false on error
bool sl_load_history(const char* filename);

// Save history to a file (one line per entry)
// Returns true on success, false on error
bool sl_save_history(const char* filename);

void sl_free_result(sl_result_t* result) {
    if (result && result->buf) {
        free(result->buf);
        result->buf = NULL;
        result->len = 0;
        result->capacity = 0;
    }
}

// ============================================================
// Utility Functions
// ============================================================

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

static sl_ull sl_utf8_decode(const char* s, sl_ll* out_len) {
    unsigned char c = (unsigned char)s[0];
    sl_ll ulen = sl_unicode_len((char)c);
    if (ulen <= 0) {
        *out_len = 1;
        return c;
    }
    sl_ull cp;
    if (ulen == 1) {
        cp = c;
    } else if (ulen == 2) {
        cp = ((sl_ull)(c & 0x1F) << 6) | (s[1] & 0x3F);
    } else if (ulen == 3) {
        cp = ((sl_ull)(c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    } else {
        cp = ((sl_ull)(c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    *out_len = ulen;
    return cp;
}

static sl_ll sl_wordwidth(const char* s) {
    unsigned char c = (unsigned char)s[0];
    if (c == '\t') return 0;
    if ((c & 0x80) == 0) return 1;

    sl_ll ulen;
    sl_ull cp = sl_utf8_decode(s, &ulen);

    if ((cp >= 0x0300 && cp <= 0x036F) ||
        (cp >= 0x0483 && cp <= 0x0489) ||
        (cp >= 0x0591 && cp <= 0x05BD) ||
        (cp >= 0x0610 && cp <= 0x061A) ||
        (cp >= 0x064B && cp <= 0x065F) ||
        (cp == 0x0670) ||
        (cp >= 0x06D6 && cp <= 0x06DC) ||
        (cp >= 0x06DF && cp <= 0x06E4) ||
        (cp >= 0x06E7 && cp <= 0x06E8) ||
        (cp >= 0x06EA && cp <= 0x06ED) ||
        (cp == 0x0711) ||
        (cp >= 0x0730 && cp <= 0x074A) ||
        (cp >= 0x07A6 && cp <= 0x07B0) ||
        (cp >= 0x0F18 && cp <= 0x0F19) ||
        (cp >= 0x17B4 && cp <= 0x17B5) ||
        (cp >= 0x1AB0 && cp <= 0x1AFF) ||
        (cp >= 0x1DC0 && cp <= 0x1DFF) ||
        (cp >= 0x20D0 && cp <= 0x20FF) ||
        (cp >= 0xFE00 && cp <= 0xFE0F) ||
        (cp >= 0xFE20 && cp <= 0xFE2F)) {
        return 0;
    }

    if ((cp >= 0x1100 && cp <= 0x115F) ||
        (cp >= 0x2E80 && cp <= 0x303E) ||
        (cp >= 0x3040 && cp <= 0x33BF) ||
        (cp >= 0x3400 && cp <= 0x4DBF) ||
        (cp >= 0x4E00 && cp <= 0xA4CF) ||
        (cp >= 0xA960 && cp <= 0xA97C) ||
        (cp >= 0xAC00 && cp <= 0xD7AF) ||
        (cp >= 0xD7B0 && cp <= 0xD7FF) ||
        (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE10 && cp <= 0xFE19) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) ||
        (cp >= 0xFF01 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) ||
        (cp >= 0x1B000 && cp <= 0x1B0FF) ||
        (cp >= 0x1F200 && cp <= 0x1F2FF) ||
        (cp >= 0x20000 && cp <= 0x2FFFF) ||
        (cp >= 0x30000 && cp <= 0x3FFFF)) {
        return 2;
    }
    return 1;
}

static sl_ull sl_calculate_col(const char* buf, sl_ull len, sl_ull start_col) {
    sl_ull col = start_col;
    for (sl_ull i = 0; i < len; ) {
        if (buf[i] == '\t') {
            col = ((col / 8) + 1) * 8;
            i++;
        } else {
            sl_ll ulen = sl_unicode_len(buf[i]);
            if (ulen <= 0) ulen = 1;
            col += (sl_ull)sl_wordwidth(buf + i);
            i += (sl_ull)ulen;
        }
    }
    return col; // absolute column
}

// ============================================================
// Line Editor State
// ============================================================
typedef struct {
    char* buf;
    sl_ull len;
    sl_ull cap;
    sl_ull pos;
    sl_ull last_crow;  // Cursor visual row within content after last refresh
    sl_ull last_rows;  // Content visual row count after last refresh
} sl_editor_t;

static bool sl_editor_init(sl_editor_t* e) {
    e->cap = 128;
    e->len = 0;
    e->pos = 0;
    e->last_crow = 0;
    e->last_rows = 1;
    e->buf = (char*)malloc(e->cap);
    if (!e->buf) return false;
    e->buf[0] = '\0';
    return true;
}

static void sl_editor_free(sl_editor_t* e) {
    free(e->buf);
    e->buf = NULL;
    e->len = 0;
    e->cap = 0;
    e->pos = 0;
}

static bool sl_editor_insert(sl_editor_t* e, char c) {
    if (e->len + 2 > e->cap) {
        sl_ull new_cap = e->cap * 2;
        char* nb = (char*)realloc(e->buf, new_cap);
        if (!nb) return false;
        e->buf = nb;
        e->cap = new_cap;
    }
    memmove(e->buf + e->pos + 1, e->buf + e->pos, e->len - e->pos + 1);
    e->buf[e->pos] = c;
    e->len++;
    e->pos++;
    return true;
}

static bool sl_editor_insert_str(sl_editor_t* e, const char* s, sl_ull n) {
    if (e->len + n + 1 > e->cap) {
        sl_ull new_cap = e->cap;
        while (new_cap <= e->len + n) new_cap *= 2;
        char* nb = (char*)realloc(e->buf, new_cap);
        if (!nb) return false;
        e->buf = nb;
        e->cap = new_cap;
    }
    memmove(e->buf + e->pos + n, e->buf + e->pos, e->len - e->pos + 1);
    memcpy(e->buf + e->pos, s, n);
    e->len += n;
    e->pos += n;
    return true;
}

static void sl_editor_delete_at(sl_editor_t* e) {
    if (e->pos < e->len) {
        sl_ll ulen = sl_unicode_len(e->buf[e->pos]);
        if (ulen <= 0) ulen = 1;
        memmove(e->buf + e->pos, e->buf + e->pos + ulen, e->len - e->pos - ulen + 1);
        e->len -= (sl_ull)ulen;
    }
}

static void sl_editor_backspace(sl_editor_t* e) {
    if (e->pos > 0) {
        e->pos--;
        while (e->pos > 0 && ((unsigned char)e->buf[e->pos] & 0xC0) == 0x80)
            e->pos--;
        sl_editor_delete_at(e);
    }
}

static void sl_editor_move_left(sl_editor_t* e) {
    if (e->pos > 0) {
        e->pos--;
        while (e->pos > 0 && ((unsigned char)e->buf[e->pos] & 0xC0) == 0x80)
            e->pos--;
    }
}

static void sl_editor_move_right(sl_editor_t* e) {
    if (e->pos < e->len) {
        e->pos++;
        while (e->pos < e->len && ((unsigned char)e->buf[e->pos] & 0xC0) == 0x80)
            e->pos++;
    }
}

static void sl_editor_home(sl_editor_t* e)  { e->pos = 0; }
static void sl_editor_end(sl_editor_t* e)   { e->pos = e->len; }

static bool sl_is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static bool sl_is_leading_whitespace(const char* buf, sl_ull pos) {
    for (sl_ull i = 0; i < pos; i++)
        if (buf[i] != ' ' && buf[i] != '\t') return false;
    return true;
}

static void sl_editor_move_word_left(sl_editor_t* e) {
    if (e->pos == 0) return;
    // Skip non-word chars to the left (separators between words)
    while (e->pos > 0 && !sl_is_word_char(e->buf[e->pos - 1])) e->pos--;
    // Now at end of a word — move to its beginning
    while (e->pos > 0 && sl_is_word_char(e->buf[e->pos - 1])) e->pos--;
}

static void sl_editor_move_word_right(sl_editor_t* e) {
    if (e->pos >= e->len) return;
    while (e->pos < e->len && !sl_is_word_char(e->buf[e->pos])) e->pos++;
    while (e->pos < e->len && sl_is_word_char(e->buf[e->pos])) e->pos++;
}

static void sl_editor_delete_word_right(sl_editor_t* e) {
    sl_ull end = e->pos;
    while (end < e->len && !sl_is_word_char(e->buf[end])) end++;
    while (end < e->len && sl_is_word_char(e->buf[end])) end++;
    if (end > e->pos) {
        memmove(e->buf + e->pos, e->buf + end, e->len - end + 1);
        e->len -= (end - e->pos);
    }
}

static void sl_editor_delete_word_left(sl_editor_t* e) {
    if (e->pos == 0) return;
    sl_ull start = e->pos;
    while (start > 0 && sl_is_word_char(e->buf[start - 1])) start--;
    while (start > 0 && !sl_is_word_char(e->buf[start - 1])) start--;
    if (start < e->pos) {
        memmove(e->buf + start, e->buf + e->pos, e->len - e->pos + 1);
        e->len -= (e->pos - start);
        e->pos = start;
    }
}

static void sl_editor_set_text(sl_editor_t* e, const char* s) {
    sl_ull slen = strlen(s);
    if (slen >= e->cap) {
        sl_ull new_cap = e->cap;
        while (new_cap <= slen) new_cap *= 2;
        char* nb = (char*)realloc(e->buf, new_cap);
        if (!nb) return;
        e->buf = nb;
        e->cap = new_cap;
    }
    memcpy(e->buf, s, slen + 1);
    e->len = slen;
    e->pos = slen;
}

// ============================================================
// History (conhost.exe provides history on Windows)
// ============================================================
#ifdef _WIN32

void sl_free_history(void) { }

bool sl_load_history(const char* filename) {
    (void)filename;
    return false;
}

bool sl_save_history(const char* filename) {
    (void)filename;
    return false;
}

#else

#define SL_HISTORY_MAX 50
static char* sl_history[SL_HISTORY_MAX];
static sl_ull sl_history_count = 0;
static sl_ll  sl_history_cursor = 0;
static char*  sl_history_saved = NULL;

void sl_free_history(void) {
    for (sl_ull i = 0; i < sl_history_count; i++) {
        free(sl_history[i]);
        sl_history[i] = NULL;
    }
    sl_history_count = 0;
    sl_history_cursor = 0;
    free(sl_history_saved);
    sl_history_saved = NULL;
}

static void sl_history_add(const char* line) {
    if (!line || line[0] == '\0') return;
    if (sl_history_count > 0 && strcmp(sl_history[sl_history_count - 1], line) == 0)
        return;

    if (sl_history_count == SL_HISTORY_MAX) {
        free(sl_history[0]);
        memmove(sl_history, sl_history + 1, (SL_HISTORY_MAX - 1) * sizeof(char*));
        sl_history_count--;
    }
    sl_history[sl_history_count] = strdup(line);
    if (sl_history[sl_history_count])
        sl_history_count++;
    sl_history_cursor = (sl_ll)sl_history_count;
}

bool sl_load_history(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return false;

    char* line = NULL;
    size_t linecap = 0;

    while (getline(&line, &linecap, f) > 0) {
        size_t l = strlen(line);
        while (l > 0 && (line[l - 1] == '\n' || line[l - 1] == '\r'))
            line[--l] = '\0';
        if (l > 0)
            sl_history_add(line);
    }

    free(line);
    fclose(f);
    return true;
}

bool sl_save_history(const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return false;

    for (sl_ull i = 0; i < sl_history_count; i++) {
        fprintf(f, "%s\n", sl_history[i]);
    }

    fclose(f);
    return true;
}

static void sl_history_up(sl_editor_t* e) {
    if (sl_history_count == 0) return;
    if (sl_history_cursor == (sl_ll)sl_history_count) {
        free(sl_history_saved);
        sl_history_saved = strdup(e->buf);
    }
    if (sl_history_cursor > 0) {
        sl_history_cursor--;
        sl_editor_set_text(e, sl_history[sl_history_cursor]);
    }
}

static void sl_history_down(sl_editor_t* e) {
    if (sl_history_count == 0) return;
    if (sl_history_cursor < (sl_ll)sl_history_count) {
        sl_history_cursor++;
        if (sl_history_cursor == (sl_ll)sl_history_count) {
            const char* restore = sl_history_saved ? sl_history_saved : "";
            sl_editor_set_text(e, restore);
        } else {
            sl_editor_set_text(e, sl_history[sl_history_cursor]);
        }
    }
}

#endif

// ============================================================
// Terminal Raw Mode (POSIX-only)
// ============================================================
#ifndef _WIN32
static struct termios sl_orig_termios;
static int  sl_raw_active = 0;
static int  sl_atexit_done = 0;

static void sl_disable_raw_mode(void) {
    if (sl_raw_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &sl_orig_termios);
        sl_raw_active = 0;
    }
}

static void sl_enable_raw_mode(void) {
    if (sl_raw_active) return;
    tcgetattr(STDIN_FILENO, &sl_orig_termios);
    struct termios raw = sl_orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    sl_raw_active = 1;

    if (!sl_atexit_done) {
        atexit(sl_disable_raw_mode);
        sl_atexit_done = 1;
    }
}
#endif

// ============================================================
// Terminal Width
// ============================================================
static int sl_terminal_width(void) {
#ifndef _WIN32
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    const char* cols = getenv("COLUMNS");
    if (cols) {
        int n = atoi(cols);
        if (n > 0) return n;
    }
#endif
    return 80;
}

// ============================================================
// Screen Refresh
// ============================================================
static void sl_refresh_line(const char* prompt, sl_editor_t* e) {
    int tw = sl_terminal_width();
    sl_ull pw = strlen(prompt);

    // Navigate from old cursor visual row to first line of content
    printf("\r");
    if (e->last_crow > 0)
        printf("\033[%lluA", e->last_crow);

    // Reprint all content (terminal handles wrapping)
    fputs(prompt, stdout);
    sl_ull col = pw;
    for (sl_ull i = 0; i < e->len; ) {
        if (e->buf[i] == '\t') {
            sl_ull next = ((col / 8) + 1) * 8;
            while (col < next) { putchar(' '); col++; }
            i++;
        } else {
            sl_ll ulen = sl_unicode_len(e->buf[i]);
            if (ulen <= 0) ulen = 1;
            for (sl_ll j = 0; j < ulen; j++)
                putchar(e->buf[i + j]);
            col += (sl_ull)sl_wordwidth(e->buf + i);
            i += (sl_ll)ulen;
        }
    }

    // Clear rest of the current line
    printf("\033[K");

    // Calculate new display dimensions
    sl_ull total_w = pw + sl_calculate_col(e->buf, e->len, 0);
    sl_ull new_rows = (total_w + tw - 1) / tw;
    if (new_rows == 0) new_rows = 1;
    sl_ull ccol = sl_calculate_col(e->buf, e->pos, pw);

    // If cursor is at the wrap-pending position (content ends exactly at the
    // terminal edge), force it onto the next line before clearing below.
    // Otherwise \033[J can have terminal-specific behaviour from that state.
    if (total_w > 0 && total_w % tw == 0)
        printf("\r\n");
    printf("\033[J");

    // Position cursor from its current row to the editing position.
    // After the clearing logic the cursor is at row new_rows (when we
    // forced the pending wrap) or new_rows-1 (when no forced wrap).
    sl_ull cur_row = (total_w > 0 && total_w % tw == 0) ? new_rows : (new_rows - 1);
    sl_ull target_crow = ccol / tw;
    sl_ull target_ccol = ccol % tw;

    if (cur_row > target_crow)
        printf("\033[%lluA", cur_row - target_crow);
    else if (cur_row < target_crow)
        printf("\033[%lluB", target_crow - cur_row);
    printf("\r");
    if (target_ccol > 0)
        printf("\033[%lluC", target_ccol);

    // Save state for next refresh
    e->last_crow = target_crow;
    e->last_rows = new_rows;

    fflush(stdout);
}

// ============================================================
// sl_input — Platform-specific implementation
// ============================================================

#ifdef _WIN32

sl_result_t sl_input(char* prompt) {
    return sl_non_interactive_readline(prompt);
}

#else // Linux

sl_result_t sl_input(char* prompt) {
    if (!isatty(STDIN_FILENO))
        return sl_non_interactive_readline(prompt);

    sl_editor_t ed;
    if (!sl_editor_init(&ed))
        return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_OUT_OF_MEMORY};

    sl_enable_raw_mode();

    fputs(prompt, stdout);
    fflush(stdout);

    for (;;) {
        int c = getchar();

        if (c == EOF) {
            if (ed.len == 0) {
                printf("\n");
                sl_disable_raw_mode();
                sl_editor_free(&ed);
                return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_EOF};
            }
            continue;
        }

        if (c == '\r' || c == '\n') {
            printf("\r\n");
            break;
        }

        if (c == 127 || c == '\b') {
            sl_editor_backspace(&ed);
            sl_refresh_line(prompt, &ed);
            continue;
        }

        if (c == '\t') {
            sl_editor_insert(&ed, '\t');
            sl_refresh_line(prompt, &ed);
            continue;
        }

        if (c == 23) {  // Ctrl+W / Ctrl+Backspace
            sl_editor_delete_word_left(&ed);
            sl_refresh_line(prompt, &ed);
            continue;
        }

        if (c == 3) {
            printf("^C\n");
            sl_disable_raw_mode();
            sl_editor_free(&ed);
            return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_EOF};
        }

        if (c == 4) {
            if (ed.len == 0) {
                printf("\n");
                sl_disable_raw_mode();
                sl_editor_free(&ed);
                return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_EOF};
            }
            continue;
        }

        if (c == 12) {
            printf("\033[2J\033[H");
            sl_refresh_line(prompt, &ed);
            continue;
        }

        if (c == '\033') {
            c = getchar();
            if (c == '[') {
                char params[16];
                sl_ull plen = 0;
                c = getchar();
                while (c != EOF && plen < sizeof(params) - 1) {
                    params[plen++] = (char)c;
                    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '~')
                        break;
                    c = getchar();
                }
                params[plen] = '\0';

                if (plen == 1) {
                    switch (params[0]) {
                        case 'A': sl_history_up(&ed);        break;
                        case 'B': sl_history_down(&ed);      break;
                        case 'C': sl_editor_move_right(&ed); break;
                        case 'D': sl_editor_move_left(&ed);  break;
                        case 'H': sl_editor_home(&ed);       break;
                        case 'F': sl_editor_end(&ed);        break;
                    }
                } else if (strcmp(params, "1~") == 0) {
                    sl_editor_home(&ed);
                } else if (strcmp(params, "3~") == 0) {
                    sl_editor_delete_at(&ed);
                } else if (strcmp(params, "4~") == 0) {
                    sl_editor_end(&ed);
                } else if (strcmp(params, "1;5D") == 0) {
                    sl_editor_move_word_left(&ed);
                } else if (strcmp(params, "1;5C") == 0) {
                    sl_editor_move_word_right(&ed);
                } else if (strcmp(params, "3;5~") == 0) {
                    sl_editor_delete_word_right(&ed);
                } else if (strcmp(params, "1;5P") == 0) {
                    sl_editor_delete_word_right(&ed);
                }
                sl_refresh_line(prompt, &ed);
            } else if (c == 'O') {
                // SS3 sequences (Home/End on some terminals: \033OH, \033OF)
                c = getchar();
                if (c == 'H') sl_editor_home(&ed);
                else if (c == 'F') sl_editor_end(&ed);
                sl_refresh_line(prompt, &ed);
            } else if (c == 'd') {
                sl_editor_delete_word_right(&ed);
                sl_refresh_line(prompt, &ed);
            }
            continue;
        }

        if (c >= 32 && c <= 126) {
            sl_editor_insert(&ed, (char)c);
            sl_refresh_line(prompt, &ed);
            continue;
        }

        if ((c & 0xC0) == 0xC0) {
            sl_ll ulen = sl_unicode_len((char)c);
            if (ulen > 1) {
                char utf[4] = {(char)c, 0, 0, 0};
                int i;
                for (i = 1; i < ulen; i++) {
                    int n = getchar();
                    if (n == EOF) break;
                    utf[i] = (char)n;
                }
                if (i == ulen) {
                    sl_editor_insert_str(&ed, utf, (sl_ull)ulen);
                    sl_refresh_line(prompt, &ed);
                }
                continue;
            }
        }
    }

    sl_disable_raw_mode();
    sl_history_add(ed.buf);

    sl_result_t res;
    res.buf       = ed.buf;
    res.len       = ed.len;
    res.capacity  = ed.cap;
    res.exception = SL_OK;
    ed.buf = NULL;
    sl_editor_free(&ed);
    return res;
}

#endif

// ============================================================
// Non-interactive readline (fallback)
// ============================================================
static sl_ull sl_readline_buffer_init_size = 128;

static sl_result_t sl_non_interactive_readline(char* prompt) {
    sl_ull cap = sl_readline_buffer_init_size;
    sl_ull l = 0;

    fputs(prompt, stdout);
    fflush(stdout);

    char* buf = (char*)malloc(cap);
    if (buf == NULL)
        return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_OUT_OF_MEMORY};

    int c = getchar();
    while (c != EOF) {
        if (c == '\n') break;
        buf[l++] = (char)c;
        if (l + 1 >= cap) {
            char* next_buf = (char*)realloc(buf, cap * 2);
            if (next_buf == NULL) {
                free(buf);
                return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_OUT_OF_MEMORY};
            }
            buf = next_buf;
            cap *= 2;
        }
        c = getchar();
    }

    if (c == EOF && l == 0) {
        free(buf);
        return (sl_result_t){NULL, 0, 0, SL_EXCEPTION_EOF};
    }

    buf[l] = 0;
    return (sl_result_t){buf, l, cap, SL_OK};
}

#endif
