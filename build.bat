@echo off
setlocal
echo ============================================
echo  Tolk Build Script - x86 + x64 (Debug + Release)
echo ============================================
echo [Preflight] Checking and installing required build tools...
echo.

:: ============================================
:: Step 1: 自动安装Chocolatey（Windows包管理器，如未安装）
:: ============================================
where choco >nul 2>&1
if %errorlevel% neq 0 (
    echo [1/6] Chocolatey not found, installing...
    powershell -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" >nul 2>&1
    set "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
    echo [1/6] Chocolatey installed successfully.
) else (
    echo [1/6] Chocolatey already installed.
)

:: 刷新环境变量，确保新安装的工具生效
call "C:\ProgramData\chocolatey\bin\RefreshEnv.cmd" >nul 2>&1

:: ============================================
:: Step 2: 安装CMake（构建系统，必须）
:: ============================================
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [2/6] CMake not found, installing...
    choco install cmake -y >nul 2>&1
    call RefreshEnv.cmd >nul 2>&1
    echo [2/6] CMake installed successfully.
) else (
    echo [2/6] CMake already installed.
)

:: ============================================
:: Step 3: 安装Visual Studio 2022 Build Tools（C++编译器，必须）
:: ============================================
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo [3/6] MSBuild / Visual Studio Build Tools not found, installing...
    choco install visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" -y >nul 2>&1
    call RefreshEnv.cmd >nul 2>&1
    echo [3/6] Visual Studio 2022 Build Tools installed successfully.
) else (
    echo [3/6] MSBuild / Visual Studio Build Tools already installed.
)

:: ============================================
:: Step 4: 安装Pandoc（文档构建，推荐）
:: ============================================
where pandoc >nul 2>&1
if %errorlevel% neq 0 (
    echo [4/6] Pandoc not found, installing...
    choco install pandoc -y >nul 2>&1
    call RefreshEnv.cmd >nul 2>&1
    echo [4/6] Pandoc installed successfully.
) else (
    echo [4/6] Pandoc already installed.
)

:: ============================================
:: Step 5: 安装.NET SDK（构建.NET wrapper，需要）
:: ============================================
where dotnet >nul 2>&1
if %errorlevel% neq 0 (
    echo [5/6] .NET SDK not found, installing...
    choco install dotnet-sdk -y >nul 2>&1
    call RefreshEnv.cmd >nul 2>&1
    echo [5/6] .NET SDK installed successfully.
) else (
    echo [5/6] .NET SDK already installed.
)

:: ============================================
:: Step 6: 安装OpenJDK 11（构建Java JAR/JNI，需要）
:: ============================================
where java >nul 2>&1
if %errorlevel% neq 0 (
    echo [6/6] OpenJDK not found, installing...
    choco install openjdk11 -y >nul 2>&1
    call RefreshEnv.cmd >nul 2>&1
    echo [6/6] OpenJDK 11 installed successfully.
) else (
    echo [6/6] OpenJDK already installed.
)

echo.
echo [Preflight] All required build tools are ready!
echo ============================================
echo.

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
