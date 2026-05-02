@echo off
setlocal

echo ============================================
echo  Tolk Build Script - x86 + x64
echo ============================================

:: x86 build
echo.
echo [1/4] Configuring x86...
cmake -B build-x86 -A Win32 -DTOLK_BUILD_JNI=OFF
if %errorlevel% neq 0 (
    echo ERROR: x86 configuration failed.
    exit /b 1
)

echo.
echo [2/4] Building x86 Release...
cmake --build build-x86 --config Release
if %errorlevel% neq 0 (
    echo ERROR: x86 build failed.
    exit /b 1
)

:: x64 build
echo.
echo [3/4] Configuring x64...
cmake -B build-x64 -A x64
if %errorlevel% neq 0 (
    echo ERROR: x64 configuration failed.
    exit /b 1
)

echo.
echo [4/4] Building x64 Release...
cmake --build build-x64 --config Release
if %errorlevel% neq 0 (
    echo ERROR: x64 build failed.
    exit /b 1
)

:: Assemble dist
echo.
echo ============================================
echo  Assembling distribution...
echo ============================================

if exist dist rmdir /s /q dist
mkdir dist\x86
mkdir dist\x64

:: x86 output
copy build-x86\dist\x86\* dist\x86\

:: x64 output
copy build-x64\dist\x64\* dist\x64\

:: .NET wrapper
if exist build-x64\src\dotnet\TolkDotNet.dll copy build-x64\src\dotnet\TolkDotNet.dll dist\

:: Java JAR
if exist build-x64\src\java\Tolk.jar copy build-x64\src\java\Tolk.jar dist\

:: Documentation
if exist build-x64\docs\README.html copy build-x64\docs\README.html dist\

:: License
if exist LICENSE*.txt copy LICENSE*.txt dist\

echo.
echo ============================================
echo  Build complete!
echo  Output: dist\
echo    dist\x86\  - 32-bit DLLs and dependencies
echo    dist\x64\  - 64-bit DLLs and dependencies
echo ============================================

endlocal
