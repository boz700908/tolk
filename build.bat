@echo off
setlocal enabledelayedexpansion
echo ============================================
echo  Tolk Build Script - x86 + x64 + ARM64 (Debug + Release)
echo ============================================
echo [Preflight] Checking and installing required build tools...
echo.

:: ============================================
:: Helper 1: Chocolatey install/upgrade with retry mechanism
:: ============================================
:CHOCO_INSTALL
set PACKAGE_NAME=%1
set RETRY_COUNT=0
set MAX_RETRIES=3

:CHOCO_LOOP_START
choco install %PACKAGE_NAME% -y >nul 2>&1
if %errorlevel% equ 0 exit /b 0

set /a RETRY_COUNT+=1
if %RETRY_COUNT% geq %MAX_RETRIES% exit /b 1

echo   Retry %RETRY_COUNT%/%MAX_RETRIES%...
timeout /t 2 /nobreak >nul
goto CHOCO_LOOP_START

:: ============================================
:: Helper 2: Version comparison function
:: Usage: call :VERSION_COMPARE "current_version" "minimum_required"
:: Returns: errorlevel 0 = meets requirement, 1 = does not meet
:: ============================================
:VERSION_COMPARE
set CURRENT_VER=%~1
set REQUIRED_VER=%~2

:: Split version string into components
for /f "tokens=1,2,3 delims=." %%a in ("%CURRENT_VER%") do (
    set CUR_MAJOR=%%a
    set CUR_MINOR=%%b
    set CUR_PATCH=%%c
)
for /f "tokens=1,2,3 delims=." %%a in ("%REQUIRED_VER%") do (
    set REQ_MAJOR=%%a
    set REQ_MINOR=%%b
    set REQ_PATCH=%%c
)

:: Handle empty values
if not defined CUR_MAJOR set CUR_MAJOR=0
if not defined CUR_MINOR set CUR_MINOR=0
if not defined CUR_PATCH set CUR_PATCH=0
if not defined REQ_MAJOR set REQ_MAJOR=0
if not defined REQ_MINOR set REQ_MINOR=0
if not defined REQ_PATCH set REQ_PATCH=0

:: Remove Java version prefix (e.g., "1." from "1.8.0")
if "%CUR_MAJOR%"=="1" if not "%CUR_MINOR%"=="" (
    set CUR_MAJOR=%CUR_MINOR%
    set CUR_MINOR=%CUR_PATCH%
    set CUR_PATCH=0
)

:: Major version comparison
if %CUR_MAJOR% gtr %REQ_MAJOR% exit /b 0
if %CUR_MAJOR% lss %REQ_MAJOR% exit /b 1

:: Minor version comparison
if %CUR_MINOR% gtr %REQ_MINOR% exit /b 0
if %CUR_MINOR% lss %REQ_MINOR% exit /b 1

:: Patch version comparison
if %CUR_PATCH% geq %REQ_PATCH% (
    exit /b 0
) else (
    exit /b 1
)

:: ============================================
:: Helper 3: Tool version detection and auto-upgrade
:: Usage: call :CHECK_TOOL "tool_name" "check_command" "min_version" "choco_pkg" "is_required"
:: ============================================
:CHECK_TOOL
set TOOL_NAME=%~1
set CHECK_CMD=%~2
set MIN_VERSION=%~3
set CHOCO_PKG=%~4
set IS_REQUIRED=%~5

:: Check if tool exists
where %TOOL_NAME% >nul 2>&1
if %errorlevel% neq 0 (
    echo [%STEP%/7] %TOOL_NAME% not found, installing...
    call :CHOCO_INSTALL %CHOCO_PKG%
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [%STEP%/7] %TOOL_NAME% installed successfully.
    ) else (
        if "%IS_REQUIRED%"=="1" (
            echo ERROR: %TOOL_NAME% installation failed.
            exit /b 1
        ) else (
            echo WARNING: %TOOL_NAME% installation failed, skipping.
        )
    )
    exit /b 0
)

:: Extract version number
for /f "tokens=*" %%v in ('%CHECK_CMD% 2^>^&1') do set VERSION_OUTPUT=%%v

:: Parse version number from output (generic pattern)
for /f "tokens=2 delims= " %%a in ("%VERSION_OUTPUT%") do set DETECTED_VERSION=%%a
:: Clean non-numeric characters from version
for /f "tokens=1 delims=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-," %%a in ("%DETECTED_VERSION%") do set DETECTED_VERSION=%%a

:: Version comparison - FORCE UPGRADE for ALL tools
call :VERSION_COMPARE "%DETECTED_VERSION%" "%MIN_VERSION%"
if %errorlevel% equ 1 (
    echo [%STEP%/7] %TOOL_NAME% %DETECTED_VERSION% ^< %MIN_VERSION%, upgrading...
    call :CHOCO_INSTALL %CHOCO_PKG%
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [%STEP%/7] %TOOL_NAME% upgraded to latest version.
    ) else (
        echo WARNING: %TOOL_NAME% upgrade failed, using current version.
    )
) else (
    echo [%STEP%/7] %TOOL_NAME% %DETECTED_VERSION% (>= %MIN_VERSION%) - OK
)
exit /b 0

:: ============================================
:: Step 1: Auto-install Chocolatey (Windows package manager)
:: ============================================
set STEP=1
where choco >nul 2>&1
if %errorlevel% neq 0 (
    echo [%STEP%/7] Chocolatey not found, installing...
    powershell -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" >nul 2>&1
    set "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
    echo [%STEP%/7] Chocolatey installed successfully.
) else (
    echo [%STEP%/7] Chocolatey already installed.
)

:: Refresh environment variables for newly installed tools
if exist "C:\ProgramData\chocolatey\bin\RefreshEnv.cmd" (
    call "C:\ProgramData\chocolatey\bin\RefreshEnv.cmd" >nul 2>&1
)

:: ============================================
:: Step 2: CMake >= 3.20 (REQUIRED)
:: ============================================
set STEP=2
call :CHECK_TOOL "cmake" "cmake --version" "3.20.0" "cmake" "1"

:: ============================================
:: Step 3: Visual Studio 2022 Build Tools (REQUIRED, MSBuild >= 17.0)
:: ============================================
set STEP=3
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo [%STEP%/7] MSBuild / Visual Studio Build Tools not found, installing...
    call :CHOCO_INSTALL visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [%STEP%/7] Visual Studio 2022 Build Tools installed successfully.
    ) else (
        echo WARNING: Build Tools installation failed, trying to continue...
    )
) else (
    :: Check MSBuild version (VS 2022 = 17.x)
    for /f "tokens=3 delims= " %%v in ('msbuild -version 2^>^&1') do set MSBUILD_VERSION=%%v
    for /f "tokens=1 delims=." %%m in ("!MSBUILD_VERSION!") do set MSBUILD_MAJOR=%%m
    
    if !MSBUILD_MAJOR! lss 17 (
        echo [%STEP%/7] WARNING: MSBuild !MSBUILD_VERSION! is older than VS 2022 (17.x)
        echo [%STEP%/7] Recommend upgrading to Visual Studio 2022 Build Tools
    ) else (
        echo [%STEP%/7] MSBuild !MSBUILD_VERSION! (VS 2022 compatible) - OK
    )
    
    :: Check vcvarsall.bat environment
    set VCVARS_FOUND=0
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        set VCVARS_FOUND=1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
        set VCVARS_FOUND=1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
        set VCVARS_FOUND=1
    )
    
    if !VCVARS_FOUND! equ 1 (
        echo [%STEP%/7] VS 2022 vcvarsall.bat detected - OK
    ) else (
        echo [%STEP%/7] WARNING: VS 2022 vcvarsall.bat not found, may need full VS 2022 installation
    )
)

:: ============================================
:: Step 4: Pandoc >= 2.18 (REQUIRED - force version check)
:: ============================================
set STEP=4
call :CHECK_TOOL "pandoc" "pandoc --version" "2.18.0" "pandoc" "1"

:: ============================================
:: Step 5: .NET SDK (REQUIRED)
:: ============================================
set STEP=5
where dotnet >nul 2>&1
if %errorlevel% neq 0 (
    echo [%STEP%/7] .NET SDK not found, installing...
    call :CHOCO_INSTALL dotnet-sdk
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [%STEP%/7] .NET SDK installed successfully.
    ) else (
        echo WARNING: .NET SDK installation failed, .NET wrapper will be skipped.
    )
) else (
    echo [%STEP%/7] .NET SDK - OK
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
:: Step 6: Java >= 11 (REQUIRED - supports --release parameter)
:: ============================================
set STEP=6
where java >nul 2>&1
if %errorlevel% neq 0 (
    echo [%STEP%/7] Java not found, installing OpenJDK 17...
    call :CHOCO_INSTALL openjdk17
    if !errorlevel! equ 0 (
        call RefreshEnv.cmd >nul 2>&1
        echo [%STEP%/7] OpenJDK 17 installed successfully.
    ) else (
        echo WARNING: Java installation failed, Java JAR will be skipped.
    )
) else (
    :: Special handling for Java version detection (outputs to stderr)
    for /f "tokens=3" %%v in ('java -version 2^>^&1 ^| findstr /i "version"') do (
        set JAVA_VERSION=%%v
        set JAVA_VERSION=!JAVA_VERSION:"=!
    )
    :: Parse Java version format (e.g., 1.8.0_302 -> 8, 11.0.12 -> 11)
    for /f "tokens=1,2 delims=._" %%a in ("!JAVA_VERSION!") do (
        if "%%a"=="1" (
            set JAVA_MAJOR=%%b
        ) else (
            set JAVA_MAJOR=%%a
        )
    )
    
    if !JAVA_MAJOR! lss 11 (
        echo [%STEP%/7] Java !JAVA_MAJOR! ^< 11 (no --release support), upgrading to OpenJDK 17...
        call :CHOCO_INSTALL openjdk17
        if !errorlevel! equ 0 (
            call RefreshEnv.cmd >nul 2>&1
            echo [%STEP%/7] OpenJDK 17 installed successfully.
        ) else (
            echo WARNING: Java upgrade failed. --release parameter may not work.
        )
    ) else (
        echo [%STEP%/7] Java !JAVA_MAJOR! (>= 11, supports --release) - OK
    )
)

:: ============================================
:: Step 7: Ninja >= 1.10 (REQUIRED - force version check)
:: ============================================
set STEP=7
call :CHECK_TOOL "ninja" "ninja --version" "1.10.0" "ninja" "1"

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
