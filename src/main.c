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
        unsigned long long len, cap;
        char* input = sl_input(">>",&len,&cap);
        if (input == NULL) {
            printf("Met EOF!\n");
            break;
        }
        printf("Got Input: \"%s\" len=%lld, cap=%lld \n",input,len,cap);
        if (strcmp(input,"exit") == 0) {
            puts("Exit! Bye~\n");
        }
    }
    
    return 0;
}