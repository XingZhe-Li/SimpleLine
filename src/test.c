#include <stdio.h>
#include <windows.h>

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    fputs("Hello",stdout);
    fflush(stdout);

    return 0;
}