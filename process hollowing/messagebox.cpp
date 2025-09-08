#include "windows.h"

int main() {
    while (1) {
        MessageBoxA(NULL,"you got fucked!","Notif", MB_OK | MB_ICONINFORMATION);
        Sleep(120000);
    }
}