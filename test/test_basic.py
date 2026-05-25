"""PTY-based tests for SimpleLine interactive readline."""

import pty
import os
import time
import sys
import re

BINARY = os.environ.get("TEST_BINARY",
    "./build/linux/x86_64/release/SimpleLine")
TIMEOUT = 5


class SimpleLineSession:
    """Spawn SimpleLine in a PTY and interact with it."""

    def __init__(self):
        self.master, slave = pty.openpty()
        self.pid = os.fork()
        if self.pid == 0:
            os.close(self.master)
            os.setsid()
            os.dup2(slave, 0)
            os.dup2(slave, 1)
            os.dup2(slave, 2)
            if slave > 2:
                os.close(slave)
            os.execv(BINARY, [BINARY])
            sys.exit(1)
        os.close(slave)
        self.buf = b""
        self.pos = 0
        # Wait for the initial banner
        self.expect(b"Simple Line:")

    def read(self, timeout=TIMEOUT):
        deadline = time.time() + timeout
        while time.time() < deadline:
            r, _, _ = select([self.master], [], [], 0.05)
            if r:
                data = os.read(self.master, 4096)
                if not data:
                    break
                self.buf += data
        return self.buf

    def write(self, data):
        os.write(self.master, data)

    def expect(self, pattern, timeout=TIMEOUT):
        """Read until pattern matches, consume buf up to the match."""
        if isinstance(pattern, str):
            pattern = pattern.encode()
        deadline = time.time() + timeout
        while time.time() < deadline:
            if pattern in self.buf[self.pos:]:
                idx = self.buf.index(pattern, self.pos)
                self.pos = idx + len(pattern)
                return True
            r, _, _ = select([self.master], [], [], 0.05)
            if r:
                data = os.read(self.master, 4096)
                if not data:
                    break
                self.buf += data
                if pattern in self.buf[self.pos:]:
                    idx = self.buf.index(pattern, self.pos)
                    self.pos = idx + len(pattern)
                    return True
        return pattern in self.buf[self.pos:]

    def sendline(self, text):
        self.write((text + "\n").encode())

    def close(self):
        try:
            os.write(self.master, b"exit\n")
        except OSError:
            pass
        time.sleep(0.2)
        try:
            os.close(self.master)
        except OSError:
            pass


# ============================================================
# Test cases
# ============================================================

def test_non_interactive():
    """Test piped (non-interactive) input."""
    import subprocess
    p = subprocess.Popen(
        [BINARY],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    out, _ = p.communicate(b"hello\nworld\nexit\n", timeout=TIMEOUT)
    assert b"Simple Line:" in out, "Missing banner"
    assert b"Got Input: \"hello\"" in out, "Missing 'hello' output"
    assert b"Got Input: \"world\"" in out, "Missing 'world' output"
    assert b"Got Input: \"exit\"" in out or b"Exit! Bye" in out, "Missing exit"
    print("  PASS non-interactive")


def test_simple_input():
    """Type a string, press Enter, verify output."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.sendline("hello")
    assert s.expect(b"Got Input: \"hello\""), f"Missing echo. buf={s.buf}"
    s.close()
    print("  PASS simple_input")


def test_empty_input():
    """Press Enter on empty line."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.sendline("")
    assert s.expect(b"Got Input: \"\""), f"Missing empty echo. buf={s.buf}"
    s.close()
    print("  PASS empty_input")


def test_exit_command():
    """Typing 'exit' should terminate."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.sendline("exit")
    assert s.expect(b"Exit! Bye"), f"Missing exit message. buf={s.buf}"
    s.close()
    print("  PASS exit_command")


def test_ctrlc_abort():
    """Ctrl+C should abort and return to prompt."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write(b"\x03")  # Ctrl+C
    assert s.expect(b"^C"), f"Missing ^C. buf={s.buf}"
    s.close()
    print("  PASS ctrlc_abort")


def test_ctrld_eof():
    """Ctrl+D on empty line should exit."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write(b"\x04")  # Ctrl+D
    assert s.expect(b"Met EOF!"), f"Missing EOF. buf={s.buf}"
    s.close()
    print("  PASS ctrld_eof")


def test_backspace():
    """Type 'helloo' then backspace once."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write(b"helloo")
    time.sleep(0.1)
    s.write(b"\x7f")  # Backspace
    s.write(b"\n")
    assert s.expect(b"Got Input: \"hello\""), f"Backspace failed. buf={s.buf}"
    s.close()
    print("  PASS backspace")


def test_tab_at_line_start():
    """Tab at line start should insert tab (indentation)."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write(b"\t\t")
    s.write(b"hello")
    s.write(b"\n")
    # Check the Got Input contains the tabs (raw buffer preserved)
    assert s.expect(b"Got Input: \"\t\t" ), f"No response. buf={s.buf}"
    s.close()
    print("  PASS tab_at_line_start")


def test_tab_in_middle():
    """Tab in the middle of text should insert a tab."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write(b"hello\thello")
    s.write(b"\n")
    assert s.expect(b"Got Input: \"hello\thello" ), f"No response. buf={s.buf}"
    s.close()
    print("  PASS tab_in_middle")


def test_history_navigation():
    """Type two lines, press Up to go back, then Down."""
    s = SimpleLineSession()
    s.expect(b">>")

    # First command
    s.sendline("first")
    s.expect(b"Got Input:")

    # Second command
    s.sendline("second")
    s.expect(b"Got Input:")

    # Press Up twice to go back to 'first'
    s.write(b"\033[A")  # Up
    time.sleep(0.05)
    s.write(b"\033[A")  # Up again
    time.sleep(0.05)
    s.write(b"\n")  # Submit 'first'

    assert s.expect(b"Got Input: \"first\""), f"History fail. buf={s.buf}"
    s.close()
    print("  PASS history_navigation")


def test_left_right_arrow():
    """Move cursor left and type to insert in middle."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"helo")
    # Move left twice, insert 'l'
    s.write(b"\033[D")  # Left
    s.write(b"\033[D")  # Left
    s.write(b"l")
    s.write(b"\n")

    assert s.expect(b"Got Input: \"hello\""), f"Arrow fail. buf={s.buf}"
    s.close()
    print("  PASS left_right_arrow")


def test_home_end():
    """Home/End keys for cursor positioning."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"o world")
    s.write(b"\033[H")   # Home (CSI)
    s.write(b"hello")
    s.write(b"\n")

    assert s.expect(b"Got Input: \"helloo world\""), f"Home fail. buf={s.buf}"
    s.close()
    print("  PASS home_end")


def test_delete_key():
    """Delete key removes character at cursor."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"helxo")
    s.write(b"\033[D")   # Left
    s.write(b"\033[D")   # Left
    s.write(b"\033[3~")  # Delete (x)
    s.write(b"\n")

    assert s.expect(b"Got Input: \"helo\""), f"Delete fail. buf={s.buf}"
    s.close()
    print("  PASS delete_key")


def test_utf8_chinese():
    """Type Chinese characters (CJK width-2)."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write("你好世界".encode("utf-8"))
    s.write(b"\n")

    assert s.expect("Got Input: \"你好世界\"".encode()), f"Chinese fail. buf={s.buf}"
    s.close()
    print("  PASS utf8_chinese")


def test_long_input():
    """Type a long line (beyond initial 128-byte buffer)."""
    s = SimpleLineSession()
    s.expect(b">>")

    text = "a" * 300
    s.sendline(text)
    assert s.expect(f"Got Input: \"{'a' * 300}\"".encode()), f"Long input fail. buf={s.buf}"
    s.close()
    print("  PASS long_input")


def test_word_left_right():
    """Ctrl+Left / Ctrl+Right word movement."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"hello world foo")
    # Move word-left 2x: should land at start of "world", then "hello"
    s.write(b"\033[1;5D")  # Ctrl+Left
    time.sleep(0.05)
    s.write(b"\033[1;5D")  # Ctrl+Left
    time.sleep(0.05)
    s.write(b"X ")
    s.write(b"\033[1;5C")  # Ctrl+Right
    time.sleep(0.05)
    s.write(b" Y")
    s.write(b"\n")

    assert s.expect(b"Got Input: \"hello X world Y foo\""), f"Word move fail. buf={s.buf}"
    s.close()
    print("  PASS word_left_right")


def test_ctrlw_delete_word():
    """Ctrl+W / Ctrl+Backspace deletes word before cursor."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"hello world foo")
    # Move left to end of "world", then Ctrl+W to delete " world"
    # Actually from end, Ctrl+W removes " foo", then " world", then "hello"
    # Let's do a simpler test: type "abc def", move to end, Ctrl+W once
    s.write(b"\n")  # Just "hello world foo"
    s.expect(b"Got Input:")

    s.write(b"abc def")
    s.write(b"\x17")  # Ctrl+W
    s.write(b"\n")

    assert s.expect(b"Got Input: \"abc\""), f"Ctrl+W fail. buf={s.buf}"
    s.close()
    print("  PASS ctrlw_delete_word")


def test_ctrlw_at_line_start():
    """Ctrl+W on empty line does nothing (no crash)."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write(b"\x17")  # Ctrl+W
    s.write(b"hello\n")
    assert s.expect(b"Got Input: \"hello\""), f"Ctrl+W at start fail. buf={s.buf}"
    s.close()
    print("  PASS ctrlw_at_line_start")


def test_ctrll_clear():
    """Ctrl+L should clear screen and redraw (check no crash)."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write(b"hello")
    s.write(b"\x0c")  # Ctrl+L
    time.sleep(0.1)
    s.write(b"\n")
    assert s.expect(b"Got Input: \"hello\""), f"Ctrl+L fail. buf={s.buf}"
    s.close()
    print("  PASS ctrll_clear")


def test_mixed_ascii_unicode():
    """Mix ASCII and CJK characters."""
    s = SimpleLineSession()
    s.expect(b">>")
    s.write("你好world你好".encode("utf-8"))
    s.write(b"\n")
    assert s.expect("Got Input: \"你好world你好\"".encode()), f"Mixed fail. buf={s.buf}"
    s.close()
    print("  PASS mixed_ascii_unicode")


def test_history_duplicate():
    """Consecutive duplicate lines should not be stored."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.sendline("unique")
    s.expect(b"Got Input:")
    s.sendline("dup")
    s.expect(b"Got Input:")
    s.sendline("dup")
    s.expect(b"Got Input:")

    # Press Up twice: should go to 'dup' (not twice 'dup')
    s.write(b"\033[A")  # Up → 'dup'
    time.sleep(0.05)
    s.write(b"\033[A")  # Up → 'unique'
    time.sleep(0.05)
    s.write(b"\n")

    assert s.expect(b"Got Input: \"unique\""), f"Dedup fail. buf={s.buf}"
    s.close()
    print("  PASS history_duplicate")


def test_submit_after_history_edit():
    """Navigate history, modify the line, submit."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.sendline("base line")
    s.expect(b"Got Input:")
    s.sendline("other")
    s.expect(b"Got Input:")

    s.write(b"\033[A")   # Up → "other"
    time.sleep(0.05)
    s.write(b"\033[A")   # Up → "base line"
    time.sleep(0.05)
    s.write(b"\033[F")   # End
    s.write(b" edited")
    s.write(b"\n")

    assert s.expect(b"Got Input: \"base line edited\""), f"History edit fail. buf={s.buf}"
    s.close()
    print("  PASS submit_after_history_edit")


def test_ctrld_with_text():
    """Ctrl+D when there's text on line does nothing (no EOF)."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"hello")
    s.write(b"\x04")  # Ctrl+D with text — should be ignored
    s.write(b"\n")

    assert s.expect(b"Got Input: \"hello\""), f"Ctrl+D with text fail. buf={s.buf}"
    s.close()
    print("  PASS ctrld_with_text")


def test_home_end_ss3():
    """Test SS3-style Home/End sequences."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"world")
    s.write(b"\033OH")   # SS3 Home
    s.write(b"hello ")
    s.write(b"\n")

    assert s.expect(b"Got Input: \"hello world\""), f"SS3 Home fail. buf={s.buf}"
    s.close()
    print("  PASS home_end_ss3")


def test_home_end_vt100():
    """Test vt100-style Home/End (ESC [1~ / ESC [4~)."""
    s = SimpleLineSession()
    s.expect(b">>")

    s.write(b"world")
    s.write(b"\033[1~")  # vt100 Home
    s.write(b"hello ")
    s.write(b"\033[4~")  # vt100 End
    s.write(b"!")
    s.write(b"\n")

    assert s.expect(b"Got Input: \"hello world!\""), f"vt100 Home/End fail. buf={s.buf}"
    s.close()
    print("  PASS home_end_vt100")


# ============================================================
# Main
# ============================================================

def main():
    tests = [
        test_non_interactive,
        test_simple_input,
        test_empty_input,
        test_exit_command,
        test_ctrlc_abort,
        test_ctrld_eof,
        test_ctrld_with_text,
        test_backspace,
        test_tab_at_line_start,
        test_tab_in_middle,
        test_history_navigation,
        test_left_right_arrow,
        test_home_end,
        test_home_end_ss3,
        test_home_end_vt100,
        test_delete_key,
        test_utf8_chinese,
        test_long_input,
        test_word_left_right,
        test_ctrlw_delete_word,
        test_ctrlw_at_line_start,
        test_ctrll_clear,
        test_mixed_ascii_unicode,
        test_history_duplicate,
        test_submit_after_history_edit,
    ]

    passed = 0
    failed = 0
    for t in tests:
        sys.stdout.flush()
        try:
            t()
            passed += 1
        except Exception as e:
            print(f"  FAIL {t.__name__}: {e}")
            failed += 1

    print(f"\n{'=' * 40}")
    print(f"Results: {passed} passed, {failed} failed, {len(tests)} total")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    # select is used in the session helper
    from select import select
    sys.exit(main())
