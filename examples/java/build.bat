@echo off
setlocal enabledelayedexpansion

:: ============================================
:: Tolk Java Example Build Script
:: ============================================

echo Building Java example...

:: Extract Tolk class from JAR
jar xf Tolk.jar com\davykager\tolk\Tolk.class
if %errorlevel% neq 0 (
    echo ERROR: Failed to extract Tolk class from JAR.
    exit /b 1
)

:: Compile Java source
javac ConsoleApp.java
if %errorlevel% neq 0 (
    echo ERROR: Java compilation failed.
    exit /b 1
)

:: Create executable JAR
jar cfe ConsoleApp.jar ConsoleApp ConsoleApp.class com\davykager\tolk\Tolk.class
if %errorlevel% neq 0 (
    echo ERROR: Failed to create JAR file.
    exit /b 1
)

:: Cleanup temporary files
if exist com rmdir /S /Q com
if exist ConsoleApp.class del ConsoleApp.class

echo Build successful: ConsoleApp.jar
endlocal
