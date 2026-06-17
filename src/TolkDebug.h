/**
 *  Product:        Tolk
 *  File:           TolkDebug.h
 *  Description:    Unified debug logging system.
 *  Copyright:      (c) 2024, Tolk contributors
 *  License:        LGPLv3
 *
 *  Features:
 *  - Release build: Logs to Windows OutputDebugString only
 *  - Debug build:   Logs to OutputDebugString + console (ERR/WRN) + log file in process directory
 *  - View with DebugView / Visual Studio Debug Output
 *  - Define TOLK_DISABLE_LOGGING to completely disable
 *  - Log file in Debug build: Tolk_Debug.log (created in calling process working directory)
 */
#ifndef _TOLK_DEBUG_H_
#define _TOLK_DEBUG_H_

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

// Log severity levels (higher = more verbose)
#define TOLK_LOG_LEVEL_ERROR   1
#define TOLK_LOG_LEVEL_WARN    2
#define TOLK_LOG_LEVEL_INFO    3
#define TOLK_LOG_LEVEL_DEBUG   4

// Default log level: INFO (shows errors, warnings, and info)
#ifndef TOLK_LOG_LEVEL
#define TOLK_LOG_LEVEL TOLK_LOG_LEVEL_INFO
#endif

#ifndef TOLK_DISABLE_LOGGING

// Global SRWLock for thread-safe logging output
// (console color changes + file writes are not thread-safe)
static SRWLOCK g_logLock = SRWLOCK_INIT;

inline void Tolk_Log(int level, const char* file, int line, const char* format, ...) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    // Build prefix: [HH:MM:SS.mmm][Tolk][LEVEL] file(line):
    const char* levelStr;
    switch (level) {
        case TOLK_LOG_LEVEL_ERROR: levelStr = "ERROR"; break;
        case TOLK_LOG_LEVEL_WARN:  levelStr = "WARN";  break;
        case TOLK_LOG_LEVEL_INFO:  levelStr = "INFO";  break;
        case TOLK_LOG_LEVEL_DEBUG: levelStr = "DEBUG"; break;
        default:                   levelStr = "UNKNOWN"; break;
    }

    char prefix[256];
    int prefixLen = snprintf(prefix, sizeof(prefix),
        "[%02d:%02d:%02d.%03d][Tolk][%s] %s(%d): ",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        levelStr, file, line);

    // Build the message body
    va_list args;
    va_start(args, format);
    char message[1024];
    int msgLen = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // 1) Always send to OutputDebugString (thread-safe, no lock needed)
    OutputDebugStringA(prefix);
    OutputDebugStringA(message);
    OutputDebugStringA("\n");

#ifdef _DEBUG
    // 2) Debug build: console output + file log — protect with lock
    AcquireSRWLockExclusive(&g_logLock);

    // Console output with color
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        WORD color;
        if (level == TOLK_LOG_LEVEL_ERROR)
            color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        else if (level == TOLK_LOG_LEVEL_WARN)
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Yellow
        else
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        SetConsoleTextAttribute(hConsole, color);
        printf("%s%s\n", prefix, message);
        // Restore default color
        SetConsoleTextAttribute(hConsole,
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    // Write to Tolk_Debug.log
    FILE* fp = nullptr;
    errno_t err = fopen_s(&fp, "Tolk_Debug.log", "a");
    if (err == 0 && fp) {
        fprintf(fp, "%s%s\n", prefix, message);
        fclose(fp);
    }

    ReleaseSRWLockExclusive(&g_logLock);
#endif
}
#define TOLK_LOG_ERROR(...)   do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_ERROR) Tolk_Log(TOLK_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define TOLK_LOG_WARN(...)    do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_WARN)  Tolk_Log(TOLK_LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define TOLK_LOG_INFO(...)    do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_INFO)  Tolk_Log(TOLK_LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define TOLK_LOG_DEBUG(...)   do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_DEBUG) Tolk_Log(TOLK_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#else
// Logging disabled — all macros compile to nothing
#define TOLK_LOG_ERROR(...)   ((void)0)
#define TOLK_LOG_WARN(...)    ((void)0)
#define TOLK_LOG_INFO(...)    ((void)0)
#define TOLK_LOG_DEBUG(...)   ((void)0)
#endif

#endif // _TOLK_DEBUG_H_
