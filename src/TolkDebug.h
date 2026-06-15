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
// Logging levels
#define TOLK_LOG_LEVEL_ERROR   1
#define TOLK_LOG_LEVEL_WARN    2
#define TOLK_LOG_LEVEL_INFO    3
#define TOLK_LOG_LEVEL_DEBUG   4
// Default log level
#ifndef TOLK_LOG_LEVEL
#define TOLK_LOG_LEVEL TOLK_LOG_LEVEL_INFO
#endif
// Compile-time enable/disable
#ifndef TOLK_DISABLE_LOGGING
// Internal log function with file and console output (Debug build only)
inline void Tolk_Log(int level, const char* file, int line, const char* format, ...)
{
    const char* levelStr = "UNK";
    WORD consoleColor = 7; // Default gray
    bool isErrorOrWarning = false;
    switch (level) {
        case TOLK_LOG_LEVEL_ERROR:
            levelStr = "ERR";
            consoleColor = 12; // Red
            isErrorOrWarning = true;
            break;
        case TOLK_LOG_LEVEL_WARN:
            levelStr = "WRN";
            consoleColor = 14; // Yellow
            isErrorOrWarning = true;
            break;
        case TOLK_LOG_LEVEL_INFO:
            levelStr = "INF";
            consoleColor = 7; // Gray
            break;
        case TOLK_LOG_LEVEL_DEBUG:
            levelStr = "DBG";
            consoleColor = 8; // Dark gray
            break;
    }
    // Get timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[64];
    _snprintf_s(timestamp, sizeof(timestamp), _TRUNCATE,
        "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    // Build prefix
    char prefix[512];
    _snprintf_s(prefix, sizeof(prefix), _TRUNCATE,
        "[%s][Tolk][%s] %s(%d): ", timestamp, levelStr, file, line);
    // Build message
    char message[1024];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);
    // Full message for OutputDebugString and file
    char fullMessage[1536];
    _snprintf_s(fullMessage, sizeof(fullMessage), _TRUNCATE,
        "%s%s\n", prefix, message);
    // Always output to Debug String
    OutputDebugStringA(fullMessage);
#ifdef _DEBUG
    // === DEBUG BUILD ONLY FEATURES ===
    // 1. Console output for ERR and WRN (colored)
    if (isErrorOrWarning) {
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        BOOL hasConsole = (hConsole != INVALID_HANDLE_VALUE) &&
            GetConsoleScreenBufferInfo(hConsole, &csbi);
        if (hasConsole) {
            SetConsoleTextAttribute(hConsole, consoleColor);
            printf("%s", fullMessage);
            SetConsoleTextAttribute(hConsole, csbi.wAttributes);
        }
        else {
            // No console attached - use OutputDebugString already done above
        }
    }
    // 2. File output to Tolk_Debug.log in process working directory (UTF-8)
    FILE* logFile = NULL;
    if (fopen_s(&logFile, "Tolk_Debug.log", "a, ccs=UTF-8") == 0 && logFile) {
        fprintf(logFile, "%s", fullMessage);
        fclose(logFile);
    }
#endif // _DEBUG
}
// Log macros
#define TOLK_LOG_ERROR(...)   do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_ERROR) Tolk_Log(TOLK_LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define TOLK_LOG_WARN(...)    do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_WARN)  Tolk_Log(TOLK_LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define TOLK_LOG_INFO(...)    do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_INFO)  Tolk_Log(TOLK_LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define TOLK_LOG_DEBUG(...)   do { if (TOLK_LOG_LEVEL >= TOLK_LOG_LEVEL_DEBUG) Tolk_Log(TOLK_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#else // TOLK_DISABLE_LOGGING
#define TOLK_LOG_ERROR(...)   ((void)0)
#define TOLK_LOG_WARN(...)    ((void)0)
#define TOLK_LOG_INFO(...)    ((void)0)
#define TOLK_LOG_DEBUG(...)   ((void)0)
#endif // TOLK_DISABLE_LOGGING
#endif // _TOLK_DEBUG_H_
