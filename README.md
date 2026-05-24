# SimpleLine

A lightweight, header-only C library providing interactive line editing on Linux, inspired by isocline and linenoise.

## Features

- **Interactive line editing** on Linux terminals with inline cursor movement
- **UTF-8 & CJK support** — full Unicode width-aware rendering
- **Line editing keys** — Left/Right, Home/End, Backspace/Delete
- **Word movement** — Ctrl+Left, Ctrl+Right
- **Word deletion** — Ctrl+Backspace (delete word left), Ctrl+Delete (delete word right)
- **History** — Up/Down arrows, 50-entry circular buffer, deduplication
- **Tab** — inserts literal tab at line start (Python-style indentation)
- **Ctrl+C** abort, **Ctrl+D** EOF on empty line, **Ctrl+L** clear screen
- **Non-interactive fallback** — automatically handles piped input
- **History persistence** — `sl_load_history()` / `sl_save_history()` APIs
- **Header-only** — drop `simpleline.h` into your project, no linker flags needed
- **Windows** — falls back to non-interactive input (conhost.exe built-in readline is superior)

## Quick Start

```bash
# Build with xmake
xmake

# Run the demo REPL
xmake run

# Run tests
xmake run_test
```

Or build with any C compiler:

```bash
cc -o simpleline src/main.c
```

## API Reference

```c
// Read a line of input (interactive on TTY, simple otherwise)
sl_result_t sl_input(char* prompt);

// Free the result buffer
void sl_free_result(sl_result_t* result);

// Free all history entries
void sl_free_history(void);

// Load history from a file
bool sl_load_history(const char* filename);

// Save history to a file
bool sl_save_history(const char* filename);
```

### `sl_result_t`

```c
typedef struct {
    char*          buf;       // The input line (null-terminated)
    unsigned long long len;   // Length of the line
    unsigned long long capacity; // Allocated capacity
    sl_exception_t exception; // SL_OK, SL_EXCEPTION_EOF, or SL_EXCEPTION_OUT_OF_MEMORY
} sl_result_t;
```

When `buf` is `NULL`, check `exception` to distinguish EOF from out-of-memory.

### Key Bindings

| Key | Action |
|-----|--------|
| Left / Right | Move cursor one character |
| Ctrl+Left / Ctrl+Right | Move one word |
| Home (CSI/SS3/vt100) | Move to beginning of line |
| End (CSI/SS3/vt100) | Move to end of line |
| Backspace | Delete character before cursor |
| Delete | Delete character at cursor |
| Ctrl+Backspace (^W) | Delete word before cursor |
| Ctrl+Delete | Delete word after cursor |
| Up / Down | Navigate history |
| Tab | Insert tab (line-start only) |
| Enter | Submit line |
| Ctrl+C | Abort (returns EOF) |
| Ctrl+D | EOF on empty line |
| Ctrl+L | Clear screen |

## Typical Usage Pattern

```c
#include "simpleline.h"

int main(void) {
    sl_load_history("history.txt");

    while (1) {
        sl_result_t r = sl_input("$ ");
        if (!r.buf) break;  // EOF or error
        process(r.buf);
        sl_free_result(&r);
    }

    sl_save_history("history.txt");
    sl_free_history();
    return 0;
}
```

## Project Structure

```
├── src/
│   ├── simpleline.h    # Header-only library (all logic)
│   └── main.c          # Demo REPL entry point
├── test/
│   └── test_basic.py   # 25 PTY-based integration tests
├── xmake.lua           # Build configuration
└── README.md
```

## License

MIT
