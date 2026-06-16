@echo off
setlocal enabledelayedexpansion
echo ============================================
echo  Tolk Build Script - x86 + x64 + ARM64 (Debug + Release)
echo ============================================
echo [Preflight] Checking and installing required build tools...
echo.

:: ============================================
:: Helper: 带重试机制的Chocolatey安装函数
:: ============================================
:CHOCO_INSTALL
set PACKAGE_NAME=%1
set RETRY_COUNT=0
set MAX_RETRIES=3

:CHOCO_RETRY
choco install %PACKAGE_NAME% -y >nul 2>&1
if %errorlevel% equ 0 (
    exit /b 0
)
set /a RETRY_COUNT+=1
if %RETRY_COUNT% lss %MAX_RETRIES% (
    echo   Retry %RETRY_COUNT%/%MAX_RETRIES%...
    timeout /t 2 /nobreak >nul
    goto CHOCO_RETRY
)
exit /b 1

:: ============================================
:: Step 1: 自动安装Chocolatey（Windows包管理器，如未安装）
:: ============================================
where choco >nul 2>&1
if %errorlevel% neq 0 (
    echo [1/7] Chocolatey not found, installing...
    powershell -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" >nul 2>&1
    set "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
    echo [1/7] Chocolatey installed successfully.
) else (
    echo [1/7] Chocolatey already installed.
)

:: 刷新环境变量，确保新安装的工具生效
if exist "C:\ProgramData\chocolatey\bin\RefreshEnv.cmd" (
    call "C:\ProgramData\chocolatey\bin\RefreshEnv.cmd" >nul 2>&1
)

:: ============================================
:: Step 2: 安装CMake（构建系统，必须）
:: ============================================
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [2/7] CMake not found, installing...
    call :CHOCO_INSTALL cmake
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [2/7] CMake installed successfully.
    ) else (
        echo WARNING: CMake installation failed, trying to continue...
    )
) else (
    echo [2/7] CMake already installed.
)

:: ============================================
:: Step 3: 安装Visual Studio 2022 Build Tools（C++编译器，必须）
:: ============================================
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo [3/7] MSBuild / Visual Studio Build Tools not found, installing...
    call :CHOCO_INSTALL visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [3/7] Visual Studio 2022 Build Tools installed successfully.
    ) else (
        echo WARNING: Build Tools installation failed, trying to continue...
    )
) else (
    echo [3/7] MSBuild / Visual Studio Build Tools already installed.
)

:: ============================================
:: Step 4: 安装Pandoc（文档构建，推荐）
:: ============================================
where pandoc >nul 2>&1
if %errorlevel% neq 0 (
    echo [4/7] Pandoc not found, installing...
    call :CHOCO_INSTALL pandoc
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [4/7] Pandoc installed successfully.
    ) else (
        echo WARNING: Pandoc installation failed, documentation will be skipped.
    )
) else (
    echo [4/7] Pandoc already installed.
)

:: ============================================
:: Step 5: 安装.NET SDK（构建.NET wrapper，需要）
:: ============================================
where dotnet >nul 2>&1
if %errorlevel% neq 0 (
    echo [5/7] .NET SDK not found, installing...
    call :CHOCO_INSTALL dotnet-sdk
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [5/7] .NET SDK installed successfully.
    ) else (
        echo WARNING: .NET SDK installation failed, .NET wrapper will be skipped.
    )
) else (
    echo [5/7] .NET SDK already installed.
)

:: ============================================
:: Step 5b: NuGet Restore for .NET projects
:: ============================================
if exist "src\dotnet\TolkDotNet.csproj" (
    echo [5b/7] Running NuGet restore for .NET project...
    dotnet restore src\dotnet\TolkDotNet.csproj >nul 2>&1
    if !errorlevel! equ 0 (
        echo [5b/7] NuGet restore completed.
    ) else (
        echo WARNING: NuGet restore failed.
    )
)

:: ============================================
:: Step 6: 安装OpenJDK 17（构建Java JAR/JNI，需要）
:: 包名验证: openjdk17 是 Chocolatey 官方正确包名
:: ============================================
where java >nul 2>&1
if %errorlevel% neq 0 (
    echo [6/7] OpenJDK not found, installing...
    call :CHOCO_INSTALL openjdk17
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [6/7] OpenJDK 17 installed successfully.
    ) else (
        echo WARNING: OpenJDK installation failed, Java JAR will be skipped.
    )
) else (
    echo [6/7] OpenJDK already installed.
)

:: ============================================
:: Step 7: 安装Ninja（可选，加速构建）
:: ============================================
where ninja >nul 2>&1
if %errorlevel% neq 0 (
    echo [7/7] Ninja not found, installing...
    call :CHOCO_INSTALL ninja
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [7/7] Ninja installed successfully.
    ) else (
        echo [7/7] Ninja installation skipped (optional).
    )
) else (
    echo [7/7] Ninja already installed.
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
echo [1/11] Configuring x86...
cmake -B build-x86 -A Win32
if %errorlevel% neq 0 (
    echo ERROR: x86 configuration failed.
    exit /b 1
)

if %BUILD_DEBUG%==1 (
echo.
echo [2/11] Building x86 Debug...
cmake --build build-x86 --config Debug
if %errorlevel% neq 0 (
    echo ERROR: x86 Debug build failed.
    exit /b 1
)
)

if %BUILD_RELEASE%==1 (
echo.
echo [3/11] Building x86 Release...
cmake --build build-x86 --config Release
if %errorlevel% neq 0 (
    echo ERROR: x86 Release build failed.
    exit /b 1
)
)

:: x64 build
echo.
echo [4/11] Configuring x64...
cmake -B build-x64 -A x64
if %errorlevel% neq 0 (
    echo ERROR: x64 configuration failed.
    exit /b 1
)

if %BUILD_DEBUG%==1 (
echo.
echo [5/11] Building x64 Debug...
cmake --build build-x64 --config Debug
if %errorlevel% neq 0 (
    echo ERROR: x64 Debug build failed.
    exit /b 1
)
)

if %BUILD_RELEASE%==1 (
echo.
echo [6/11] Building x64 Release...
cmake --build build-x64 --config Release
if %errorlevel% neq 0 (
    echo ERROR: x64 Release build failed.
    exit /b 1
)
)

:: ARM64 build
echo.
echo [7/11] Configuring ARM64...
cmake -B build-arm64 -A ARM64
if %errorlevel% neq 0 (
    echo WARNING: ARM64 toolchain not available, skipping ARM64 build.
    goto skip_arm64
)

if %BUILD_DEBUG%==1 (
echo.
echo [8/11] Building ARM64 Debug...
cmake --build build-arm64 --config Debug
if %errorlevel% neq 0 (
    echo WARNING: ARM64 Debug build failed, skipping.
)
)

if %BUILD_RELEASE%==1 (
echo.
echo [9/11] Building ARM64 Release...
cmake --build build-arm64 --config Release
if %errorlevel% neq 0 (
    echo WARNING: ARM64 Release build failed, skipping.
)
)

:skip_arm64

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
mkdir dist\arm64\Debug
mkdir dist\arm64\Release

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

:: ARM64 Debug output
if %BUILD_DEBUG%==1 (
if exist build-arm64\dist\arm64-Debug copy build-arm64\dist\arm64-Debug\* dist\arm64\Debug\
)

:: ARM64 Release output
if %BUILD_RELEASE%==1 (
if exist build-arm64\dist\arm64-Release copy build-arm64\dist\arm64-Release\* dist\arm64\Release\
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
echo    dist\x86\Debug\     - 32-bit x86 Debug DLLs
echo    dist\x86\Release\   - 32-bit x86 Release DLLs
echo    dist\x64\Debug\     - 64-bit x64 Debug DLLs
echo    dist\x64\Release\   - 64-bit x64 Release DLLs
echo    dist\arm64\Debug\   - ARM64 Debug DLLs (NVDA only)
echo    dist\arm64\Release\ - ARM64 Release DLLs (NVDA only)
echo.
echo  Debug Build Features:
echo    - Tolk_Debug.log written to calling process working directory
echo    - ERR/WRN messages printed to console (colored)
echo    - All logs sent to OutputDebugString
echo.
echo  ARM64 Notes:
echo    - Only NVDA screen reader supports ARM64 natively
echo    - Other drivers (JAWS, SAPI, etc.) will auto-disable on ARM64
echo.
echo  Usage:
echo    build.bat          - Build both Debug and Release
echo    build.bat debug    - Build Debug only
echo    build.bat release  - Build Release only
echo ============================================
endlocal
