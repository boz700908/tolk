@echo off
setlocal
echo ============================================
echo  Tolk Build Script - x86 + x64 (Debug + Release)
echo ============================================
:: Parse command line arguments
set BUILD_DEBUG=1
set BUILD_RELEASE=1
if "%1"=="release" set BUILD_DEBUG=0
if "%1"=="debug" set BUILD_RELEASE=0
:: x86 build
echo.
echo [1/8] Configuring x86...
cmake -B build-x86 -A Win32
if %errorlevel% neq 0 (
    echo ERROR: x86 configuration failed.
    exit /b 1
)
if %BUILD_DEBUG%==1 (
echo.
echo [2/8] Building x86 Debug...
cmake --build build-x86 --config Debug
if %errorlevel% neq 0 (
    echo ERROR: x86 Debug build failed.
    exit /b 1
)
)
if %BUILD_RELEASE%==1 (
echo.
echo [3/8] Building x86 Release...
cmake --build build-x86 --config Release
if %errorlevel% neq 0 (
    echo ERROR: x86 Release build failed.
    exit /b 1
)
)
:: x64 build
echo.
echo [4/8] Configuring x64...
cmake -B build-x64 -A x64
if %errorlevel% neq 0 (
    echo ERROR: x64 configuration failed.
    exit /b 1
)
if %BUILD_DEBUG%==1 (
echo.
echo [5/8] Building x64 Debug...
cmake --build build-x64 --config Debug
if %errorlevel% neq 0 (
    echo ERROR: x64 Debug build failed.
    exit /b 1
)
)
if %BUILD_RELEASE%==1 (
echo.
echo [6/8] Building x64 Release...
cmake --build build-x64 --config Release
if %errorlevel% neq 0 (
    echo ERROR: x64 Release build failed.
    exit /b 1
)
)
:: Assemble dist
echo.
echo ============================================
echo  Assembling distribution...
echo ============================================
if exist dist rmdir /s /q dist
mkdir dist\x86\Debug
mkdir dist\x86\Release
mkdir dist\x64\Debug
mkdir dist\x64\Release
:: x86 Debug output
if %BUILD_DEBUG%==1 (
if exist build-x86\dist\x86-Debug copy build-x86\dist\x86-Debug\* dist\x86\Debug\
)
:: x86 Release output
if %BUILD_RELEASE%==1 (
if exist build-x86\dist\x86-Release copy build-x86\dist\x86-Release\* dist\x86\Release\
)
:: x64 Debug output
if %BUILD_DEBUG%==1 (
if exist build-x64\dist\x64-Debug copy build-x64\dist\x64-Debug\* dist\x64\Debug\
)
:: x64 Release output
if %BUILD_RELEASE%==1 (
if exist build-x64\dist\x64-Release copy build-x64\dist\x64-Release\* dist\x64\Release\
)
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
echo    dist\x86\Debug\    - 32-bit Debug DLLs (with file logging + console output)
echo    dist\x86\Release\  - 32-bit Release DLLs
echo    dist\x64\Debug\    - 64-bit Debug DLLs (with file logging + console output)
echo    dist\x64\Release\  - 64-bit Release DLLs
echo.
echo  Debug Build Features:
echo    - Tolk_Debug.log written to calling process working directory
echo    - ERR/WRN messages printed to console (colored)
echo    - All logs sent to OutputDebugString
echo.
echo  Usage:
echo    build.bat          - Build both Debug and Release
echo    build.bat debug    - Build Debug only
echo    build.bat release  - Build Release only
echo ============================================
endlocal
