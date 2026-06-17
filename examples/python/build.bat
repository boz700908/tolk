@echo off
setlocal enabledelayedexpansion

:: ============================================
:: Tolk Python Example Build Script
:: ============================================

echo Building Python example...
python -m py_compile ConsoleApp.py
if %errorlevel% neq 0 (
    echo ERROR: Python compilation failed.
    exit /b 1
)

echo Build successful: ConsoleApp.pyc
endlocal
