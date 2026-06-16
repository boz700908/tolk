@echo off
setlocal enabledelayedexpansion

:: ============================================
:: Tolk PureBasic Example Build Script
:: ============================================

echo Building PureBasic example...
pbcompiler /QUIET /UNICODE /CONSOLE /USER /SSE2 /EXE ConsoleApp.exe ConsoleApp.pb
if %errorlevel% neq 0 (
    echo ERROR: PureBasic compilation failed.
    exit /b 1
)

echo Build successful: ConsoleApp.exe
endlocal
