# SimpleLine Source Code Documentation

## Architecture Overview

SimpleLine is a **header-only C library** (`simpleline.h`) that implements interactive line editing for Linux terminals. All code lives in a single header file guarded by `#ifndef SIMPLELINE_H`. Static functions are used for internal helpers to avoid duplicate symbol errors when the header is included in multiple translation units.

### File Layout

```
simpleline.h (~690 lines)
│
├── Public type definitions    (sl_result_t, sl_exception_t)
├── Public function declarations
│   ├── sl_input()
│   ├── sl_free_result()
│   ├── sl_free_history()
│   ├── sl_load_history()
│   └── sl_save_history()
│
├── Internal utility functions
│   ├── sl_unicode_len()       — UTF-8 byte sequence length
│   ├── sl_utf8_decode()       — Decode UTF-8 to codepoint
│   ├── sl_wordwidth()         — Unicode character display width
│   └── sl_calculate_col()     — Visual column position
│
├── Line editor (sl_editor_t)
│   ├── init/free/insert/delete
│   ├── Cursor movement (left, right, word-left, word-right, home, end)
│   └── Word deletion (left, right)
│
├── History management
│   ├── sl_history_add()       — Add line with dedup, circular eviction
│   ├── sl_history_up/down()   — Navigate with saved-buffer restore
│   ├── sl_load_history()      — Load from file
│   └── sl_save_history()      — Save to file
│
├── Terminal raw mode
│   ├── sl_enable_raw_mode()   — termios manipulation
│   └── sl_disable_raw_mode()  — Restore original settings
│
├── Screen refresh
│   └── sl_refresh_line()      — Redraw with ANSI escape codes
│
├── sl_input() [Linux]          — Main interactive readline loop
├── sl_input() [Windows stub]
└── sl_non_interactive_readline() — Fallback for piped input
```

---

## Key Internal Types

### `sl_editor_t`

The core editor state, used only within `sl_input()`:

```c
typedef struct {
    char* buf;       // The editable line buffer (null-terminated)
    sl_ull len;      // Current string length
    sl_ull cap;      // Allocated capacity
    sl_ull pos;      // Cursor position (byte index)
} sl_editor_t;
```

- Buffer starts at 128 bytes and doubles on demand.
- `pos` is always a byte index into `buf`, not a display column.
- Multi-byte UTF-8 characters are stored as-is; cursor movement skips continuation bytes (bytes matching `0xC0 & 0x80`).

### `sl_result_t`

The return type for `sl_input()`:

```c
typedef struct {
    char* buf;
    sl_ull len;
    sl_ull capacity;
    sl_exception_t exception;
} sl_result_t;
```

On success, `buf` is heap-allocated and owned by the caller. On EOF or out-of-memory, `buf` is NULL and `exception` indicates the reason.

---

## UTF-8 Handling

### Byte sequence detection (`sl_unicode_len`)

| First byte | Sequence length |
|------------|----------------|
| `0x00–0x7F` | 1 (ASCII) |
| `0xC0–0xDF` | 2 |
| `0xE0–0xEF` | 3 |
| `0xF0–0xF7` | 4 |
| `0x80–0xBF` | 0 (continuation byte) |
| Other | -1 (invalid) |

### Codepoint decoding (`sl_utf8_decode`)

Extracts the Unicode codepoint from 1–4 byte sequences. Used by `sl_wordwidth()` for character width classification.

### Display width (`sl_wordwidth`)

Returns:
- `0` — Combining marks (U+0300–U+036F, etc.) and tab characters
- `1` — ASCII and most non-CJK characters
- `2` — CJK ideographs, Hangul, and other wide characters

Width-2 characters cause the terminal cursor to advance by 2 columns. This is critical for correct cursor positioning after refresh.

### Visual column calculation (`sl_calculate_col`)

Walks the buffer byte-by-byte, accumulating display columns:

- Tab (`\t`): snaps to the next 8-column boundary from current position
- Other: adds `sl_wordwidth()` result

Takes a `start_col` parameter to account for the prompt width, so tab stops align correctly.

---

## Terminal Raw Mode

The `sl_enable_raw_mode()` function modifies termios attributes on STDIN_FILENO:

| Flag | Change | Effect |
|------|--------|--------|
| `ICANON` | Cleared | Disable line buffering — read each byte immediately |
| `ECHO` | Cleared | Disable local echo — we handle display ourselves |
| `ISIG` | Cleared | Disable signal generation — Ctrl+C sends `\x03` as data |
| `IEXTEN` | Cleared | Disable `\x17` (Ctrl+W) being consumed by the driver |
| `IXON` | Cleared | Disable software flow control (`\x13`/`\x11`) |
| `ICRNL` | Cleared | Don't translate CR to NL on input |
| `OPOST` | Cleared | Disable output processing (no `\n` → `\r\n` translation) |
| `VMIN` | 1 | Minimum bytes for read to return |
| `VTIME` | 0 | No timeout on read |

Raw mode is registered with `atexit()` as a safety net in case the program terminates abnormally.

---

## Escape Sequence Parsing

The main input loop in `sl_input()` reads bytes one at a time via `getchar()`. When an escape byte (`\033`, 0x1B) is received, the parser reads subsequent bytes:

```
\033
 ├── [  → CSI sequence → read until letter or '~'
 │    ├── A/ B           → Up / Down arrow (history)
 │    ├── C/ D           → Right / Left arrow
 │    ├── H/ F           → Home / End (xterm)
 │    ├── 1~             → Home (vt100)
 │    ├── 3~             → Delete
 │    ├── 4~             → End (vt100)
 │    ├── 1;5D/ 1;5C     → Ctrl+Left / Ctrl+Right
 │    ├── 3;5~/ 1;5P     → Ctrl+Delete
 │    └── (other)        → Ignored
 ├── O  → SS3 sequence → H or F → Home or End
 └── d  → Ctrl+Delete (some terminals)
```

Parameters between `[` and the terminator are accumulated in a `char params[16]` buffer and compared as strings. This is simpler and more robust than parsing numeric parameters individually.

---

## Line Buffer Operations

### Insertion

`sl_editor_insert()` inserts a single byte at `buf[pos]`, shifting the rest right by one position via `memmove`. Buffer grows by 2x if needed.

`sl_editor_insert_str()` is the multi-byte variant used for UTF-8 characters (up to 4 bytes).

### Deletion

- `sl_editor_delete_at()`: removes the character at `buf[pos]`, using `sl_unicode_len()` to determine the byte width
- `sl_editor_backspace()`: decrements `pos` (skipping continuation bytes), then calls `sl_editor_delete_at()`
- `sl_editor_delete_word_left()`: finds the start of the word (or separator) before cursor and deletes up to `pos`
- `sl_editor_delete_word_right()`: finds the end of the word (or separator) after cursor and deletes from `pos`

### Word boundaries

A "word character" is defined as `[a-zA-Z0-9_]`. Everything else is a separator. Word movement uses a two-pass approach:
1. Skip backward over non-word chars (separators between cursor and word)
2. Skip backward over word chars (to the beginning of the word)

---

## History Management

History is stored as a flat array of 50 `char*` pointers:

```c
static char* sl_history[SL_HISTORY_MAX];  // 50 entries
static sl_ull sl_history_count;
static sl_ll  sl_history_cursor;
static char*  sl_history_saved;  // Temporary buffer for editing in progress
```

### Circular eviction

When the history array is full (50 entries) and a new entry is added:
1. `free(sl_history[0])` — discard the oldest entry
2. `memmove(sl_history, sl_history + 1, ...)` — shift remaining entries left
3. Decrement count, then append at the new slot

### Deduplication

`sl_history_add()` skips empty lines and consecutive duplicates (comparing against `sl_history[count - 1]`).

### Navigation

- `sl_history_up()`: saves the current editor buffer to `sl_history_saved` (if not already saved), then moves cursor backward and loads the history entry.
- `sl_history_down()`: moves cursor forward; when past the last entry, restores `sl_history_saved` or an empty string.

### Persistence

- `sl_load_history()`: reads a file line by line (via `getline`) and feeds each non-empty line to `sl_history_add()`.
- `sl_save_history()`: writes all entries to a file, one per line.

---

## Screen Refresh

`sl_refresh_line()` redraws the current line from scratch:

```c
printf("\r%s%s\033[K", prompt, e->buf);       // Carriage return + prompt + line content + clear to end
printf("\r\033[%lluC", ccol);                  // Move cursor back to correct column
fflush(stdout);
```

This approach is simple and reliable: redraw everything, then reposition. The `\033[K` clears any characters after the cursor on the display line, and `\033[%lluC` moves the cursor forward by `ccol` columns.

The column calculation uses `sl_calculate_col()` which accounts for:
- Prompt width (added as `start_col`)
- Tab stops (8-column boundaries)
- CJK wide characters (2 columns each)

---

## Main Input Loop (`sl_input`)

The `sl_input()` function on Linux:

1. Check `isatty(STDIN_FILENO)` — if not a TTY, delegate to `sl_non_interactive_readline()`
2. Initialize the editor state (`sl_editor_t`)
3. Enable raw mode
4. Print the prompt
5. Loop reading characters:
   - **Printable ASCII** (32–126): insert and refresh
   - **UTF-8 multi-byte** (0xC0–0xFF): read continuation bytes, insert string
   - **Control chars**: Enter, Backspace, Tab, Ctrl+C/D/L/W
   - **Escape sequences**: parse and dispatch to editor functions
6. On Enter: disable raw mode, add to history, return the buffer

### Non-interactive fallback

`sl_non_interactive_readline()` reads characters until newline or EOF using `getchar()`, with a dynamically growing buffer. The prompt is printed to stdout before reading.

---

## Testing

The test suite (`test/test_basic.py`) uses Python's `pty` module to spawn the SimpleLine binary in a pseudo-terminal, simulating keystrokes and verifying output. This tests the full interactive path without requiring a physical terminal.

25 test cases cover:
- Non-interactive (piped) input
- Basic text entry and submission
- Cursor movement (arrows, Home/End, word movement)
- Editing (Backspace, Delete, word deletion)
- History navigation and deduplication
- Tab (leading whitespace only)
- UTF-8 Chinese characters
- Ctrl+C, Ctrl+D, Ctrl+L, Ctrl+W
- Long input (>128 bytes, triggering buffer reallocation)

Run with: `xmake run_test` or `python3 test/test_basic.py`
