# SimpleLine Usage Guide

## Building

### With xmake

```bash
xmake               # Build release
xmake config -m debug && xmake   # Build debug
xmake run           # Run the demo REPL
xmake run_test      # Run the test suite
```

### With plain cc/gcc/clang

```bash
cc -o simpleline src/main.c -Wall
```

No extra linker flags are needed. On Linux, `termios` and `unistd` are included internally by `simpleline.h`.

### Cross-platform note

On Windows, `simpleline.h` falls back to a simple `getchar()` read loop — the Windows console host (conhost.exe) already provides built-in line editing, so no raw-mode manipulation is needed or attempted.

## Integration: Using simpleline.h in your own project

Since `simpleline.h` is header-only, just copy it into your source tree and include it:

```c
#include "simpleline.h"
```

One translation unit must include it exactly once. It uses the header guard (`SIMPLELINE_H`) to prevent double inclusion.

### Minimal REPL

```c
#include <stdio.h>
#include "simpleline.h"

int main(void) {
    while (1) {
        sl_result_t r = sl_input("> ");
        if (!r.buf) {
            if (r.exception == SL_EXCEPTION_EOF)
                puts("EOF");
            else
                puts("Out of memory");
            break;
        }
        printf("You said: %s\n", r.buf);
        sl_free_result(&r);
    }
    return 0;
}
```

### With history persistence

```c
#include "simpleline.h"

int main(void) {
    sl_load_history(".myapp_history");

    while (1) {
        sl_result_t r = sl_input("$ ");
        if (!r.buf) break;

        if (r.buf[0] != '\0')
            printf("echo: %s\n", r.buf);

        sl_free_result(&r);
    }

    sl_save_history(".myapp_history");
    sl_free_history();
    return 0;
}
```

## API Reference

### `sl_result_t sl_input(char* prompt)`

Reads one line of input.

- **Interactive mode** (stdin is a TTY on Linux): enables raw terminal mode, displays the prompt, and provides full line editing.
- **Non-interactive mode** (piped input or Windows): reads from stdin until newline or EOF, echoing the prompt to stdout.

**Parameters:**
- `prompt` — A null-terminated string displayed before the input area. Passed by pointer (not copied).

**Returns:** An `sl_result_t` struct.

**Error handling:**

```c
sl_result_t r = sl_input("> ");
if (r.buf == NULL) {
    if (r.exception == SL_EXCEPTION_EOF) {
        // EOF (Ctrl+D on empty line, or Ctrl+C, or piped input EOF)
    } else {
        // SL_EXCEPTION_OUT_OF_MEMORY — malloc/realloc failed
    }
}
// On success, r.exception == SL_OK
```

The caller owns `r.buf` and must call `sl_free_result(&r)` to free it.

---

### `void sl_free_result(sl_result_t* result)`

Frees the buffer inside a result and zeroes out the struct fields. Safe to call on a zero-initialized struct (checks for NULL).

---

### `void sl_free_history(void)`

Frees all stored history entries and resets the history state. Call this before program exit if you've used interactive input, to avoid memory leaks.

---

### `bool sl_load_history(const char* filename)`

Loads history from a file, one line per entry. Lines are added via the same deduplication logic as interactive input.

**Returns:** `true` on success, `false` if the file could not be opened (not an error if file doesn't exist).

---

### `bool sl_save_history(const char* filename)`

Writes all history entries to a file, one per line. Overwrites the file if it exists.

**Returns:** `true` on success, `false` if the file could not be opened for writing.

---

### `sl_result_t` fields

| Field | Type | Description |
|-------|------|-------------|
| `buf` | `char*` | The input line, or NULL on error |
| `len` | `unsigned long long` | String length (not including null terminator) |
| `capacity` | `unsigned long long` | Allocated buffer capacity |
| `exception` | `sl_exception_t` | `SL_OK` (0), `SL_EXCEPTION_EOF` (1), or `SL_EXCEPTION_OUT_OF_MEMORY` (2) |

## Key Binding Reference

### Cursor movement

| Input | Sequence | Action |
|-------|----------|--------|
| Left arrow | `\033[D` | One character left |
| Right arrow | `\033[C` | One character right |
| Ctrl+Left | `\033[1;5D` | One word left |
| Ctrl+Right | `\033[1;5C` | One word right |
| Home | `\033[H` / `\033OH` / `\033[1~` | Beginning of line |
| End | `\033[F` / `\033OF` / `\033[4~` | End of line |

### Editing

| Input | Sequence | Action |
|-------|----------|--------|
| Backspace | `\x7f` / `\b` | Delete character before cursor |
| Delete | `\033[3~` | Delete character at cursor |
| Ctrl+Backspace | `\x17` (Ctrl+W) | Delete word before cursor |
| Ctrl+Delete | `\033[3;5~` / `\033[1;5P` / `\033 d` | Delete word after cursor |
| Tab | `\x09` | Insert tab (line-start only) |
| Enter | `\r` / `\n` | Submit line |

### Control characters

| Input | Action |
|-------|--------|
| Ctrl+C | Abort (returns EOF) |
| Ctrl+D (empty line) | EOF (returns NULL with `SL_EXCEPTION_EOF`) |
| Ctrl+D (with text) | Ignored |
| Ctrl+L | Clear screen and redraw |
| Ctrl+W | Delete word before cursor (same as Ctrl+Backspace) |

## Limitations

- **History limit**: 50 entries (compile-time constant `SL_HISTORY_MAX`)
- **Tab**: Only inserted at line start (for indentation), ignored elsewhere
- **Windows**: Raw-mode editing is not implemented; uses simple `getchar()` input
- **Signal handling**: Ctrl+C is caught but no custom signal handler is installed; raw mode is restored via `atexit`
