// Universal Headers
#include <stdio.h>
#include <string.h>
#include "simpleline.h"

// Platform Related Headers
#ifdef _WIN32
#include <windows.h>
#endif

// Entrypoint for The Executable
int main(int argc, char **argv) {
#ifdef _WIN32
    // Equivalent to chcp 65001, switch codepage to UTF8!
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    puts("Simple Line: A Truncated Version Of Isocline & Linenoise.");

    for (;;) {
        sl_result_t result = sl_input(">>");
        if (result.buf == NULL) {
            if (result.exception == SL_EXCEPTION_EOF)
                puts("Met EOF!");
            else
                puts("Out of memory!");
            break;
        }
        printf("Got Input: \"%s\" len=%lld, cap=%lld \n",result.buf,result.len,result.capacity);
        if (strcmp(result.buf,"exit") == 0) {
            puts("Exit! Bye~\n");
            sl_free_history();
            return 0;
        }
        sl_free_result(&result);
    }

    sl_free_history();
    return 0;
}