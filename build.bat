@echo off
setlocal enabledelayedexpansion

:: ============================================
::  Tolk Build Script — Zero-dependency bootstrap
::  x86 + x64 + ARM64 (Debug + Release)
::  Usage: build.bat [debug|release] [--clean] [--x86] [--x64] [--arm64]
::  --x86/--x64/--arm64 : build only the specified architecture(s)
::    (omit all to build all three architectures)
::  Works on a completely clean Windows machine.
:: ============================================

:: ---------- Parse arguments ----------
set BUILD_CONFIG=Release
set DO_CLEAN=0
set BUILD_X86=0
set BUILD_X64=0
set BUILD_ARM64=0
set ARCH_SPECIFIED=0
:PARSE_ARGS
if "%~1"=="" goto :START
if /i "%~1"=="debug"   set BUILD_CONFIG=Debug
if /i "%~1"=="release" set BUILD_CONFIG=Release
if /i "%~1"=="--clean" set DO_CLEAN=1
if /i "%~1"=="--x86"   set ARCH_SPECIFIED=1 & set BUILD_X86=1
if /i "%~1"=="--x64"   set ARCH_SPECIFIED=1 & set BUILD_X64=1
if /i "%~1"=="--arm64" set ARCH_SPECIFIED=1 & set BUILD_ARM64=1
shift
goto :PARSE_ARGS

:START
:: Detect CI environment
set IS_CI=0
if defined CI             set IS_CI=1
if defined APPVEYOR       set IS_CI=1
if defined GITHUB_ACTIONS set IS_CI=1
:: CI always does a clean build
if %IS_CI%==1 set DO_CLEAN=1

echo ============================================
echo  Tolk Build Script
echo  Config: %BUILD_CONFIG%  CI: %IS_CI%  Clean: %DO_CLEAN%
echo ============================================

:: ---------- Admin check (skip on CI — GitHub Actions has no admin) ----------
if %IS_CI%==1 goto :SKIP_ADMIN
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo ERROR: This script requires Administrator privileges.
    echo Chocolatey and Visual Studio Build Tools need admin rights.
    echo Please right-click the Command Prompt and select "Run as administrator".
    echo.
    exit /b 1
)
echo [Admin] Running with administrator privileges.
:SKIP_ADMIN

:: ---------- Clean build directories ----------
if %DO_CLEAN%==1 (
    echo [Clean] Removing stale build directories...
    for %%D in (build-x86 build-x64 build-arm64 dist) do (
        if exist "%%D" (
            echo   Removing %%D\
            rmdir /s /q "%%D" 2>nul
        )
    )
    echo [Clean] Done.
)

:: ============================================
:: Helper: Safe environment refresh
:: ============================================
:SAFE_REFRESH_ENV
for %%P in (
    "%ChocolateyInstall%\bin\RefreshEnv.cmd"
    "%ProgramData%\chocolatey\bin\RefreshEnv.cmd"
    "%ALLUSERSPROFILE%\chocolatey\bin\RefreshEnv.cmd"
) do (
    if exist %%P (
        call %%P >nul 2>&1
        exit /b 0
    )
)
exit /b 0

:: ============================================
:: Helper: Ensure choco is on PATH. Called after
:: Chocolatey install or when choco is not found.
:: ============================================
:ENSURE_CHOCO_PATH
for %%P in (
    "%ChocolateyInstall%\bin\choco.exe"
    "%ProgramData%\chocolatey\bin\choco.exe"
    "%ALLUSERSPROFILE%\chocolatey\bin\choco.exe"
    "C:\ProgramData\chocolatey\bin\choco.exe"
) do (
    if exist %%P (
        for %%D in ("%%~dpP.") do set "PATH=!PATH!;%%~dpP"
        exit /b 0
    )
)
exit /b 1

:: ============================================
:: Helper: Chocolatey install with retry
:: VS packages get verbose output; others are quiet.
:: ============================================
:CHOCO_INSTALL
setlocal
set "IS_VS=%~1"
set /a TRY=0
shift
:CHOCO_RETRY
if "%IS_VS%"=="--vs" (
    echo   ^(this may take 10-30 minutes — downloading Visual Studio...^)
    shift
    choco install %* -y --no-progress --allow-downgrade
) else (
    choco install %* -y --no-progress --limit-output --allow-downgrade
)
set RC=%errorlevel%
:: 3010 = reboot required (success for VS installs)
if %RC% equ 3010 set RC=0
if %RC% equ 0 (
    call :SAFE_REFRESH_ENV
    endlocal & exit /b 0
)
set /a TRY+=1
if %TRY% lss 3 (
    ping -n 3 127.0.0.1 >nul
    goto :CHOCO_RETRY
)
endlocal & exit /b %RC%

:: ============================================
:: Helper: Locate MSBuild using vswhere or fallback
:: ============================================
:LOCATE_MSBUILD
setlocal
:: Try vswhere first (installed with VS Build Tools)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%p in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe"`) do (
        set "MSBUILD_PATH=%%p"
        goto :MSBUILD_FOUND
    )
)
:: Fallback: search known paths
for %%P in (
    "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
) do (
    if exist %%P (
        set "MSBUILD_PATH=%%P"
        goto :MSBUILD_FOUND
    )
)
endlocal & exit /b 1

:MSBUILD_FOUND
for %%P in ("%MSBUILD_PATH%\..") do set "MSBUILD_DIR=%%~dpP"
set "PATH=%MSBUILD_DIR%;%PATH%"
endlocal & set "PATH=%MSBUILD_DIR%;%PATH%" & exit /b 0

:: ============================================
:: Helper: Check tool version against minimum
:: ============================================
:CHECK_TOOL
setlocal
set "TOOL_NAME=%~1"
set "MIN_VERSION=%~2"
set "VERSION_ARG=%~3"
set "CHOCO_PKG=%~4"
set "IS_REQUIRED=%~5"
set "CI_SKIP=%~6"
if "%TOOL_NAME%"=="" endlocal & exit /b 0

:: On GitHub Actions, skip all tool installation (tools provided by setup actions)
if defined GITHUB_ACTIONS (
    echo [%STEP%/7] %TOOL_NAME%: using GitHub Actions-provided tool
    endlocal & exit /b 0
)

:: On CI, skip VS/MSBuild installation (VS 2022 is pre-installed)
if %IS_CI%==1 if "%CI_SKIP%"=="1" (
    echo [%STEP%/7] %TOOL_NAME%: using pre-installed Visual Studio 2022
    endlocal & exit /b 0
)

:: Check if tool exists
where "%TOOL_NAME%" >nul 2>&1
if %errorlevel% neq 0 (
    echo [%STEP%/7] %TOOL_NAME% not found, installing...
    if "%CI_SKIP%"=="1" (
        :: VS install: show progress, it takes a long time
        call :CHOCO_INSTALL --vs %CHOCO_PKG%
    ) else (
        call :CHOCO_INSTALL %CHOCO_PKG%
    )
    if !errorlevel! equ 0 (
        echo [%STEP%/7] %TOOL_NAME% installed.
        :: For MSBuild: locate it and add to PATH
        if "%CI_SKIP%"=="1" call :LOCATE_MSBUILD
    ) else (
        if "%IS_REQUIRED%"=="1" (
            echo ERROR: %TOOL_NAME% installation failed.
            endlocal & exit /b 1
        ) else (
            echo WARNING: %TOOL_NAME% installation failed, skipping.
        )
    )
    endlocal & exit /b 0
)

:: For MSBuild on a clean machine (not CI): ensure it's on PATH
if "%CI_SKIP%"=="1" if %IS_CI%==0 call :LOCATE_MSBUILD

:: Check version
for /f "tokens=*" %%v in ('%TOOL_NAME% %VERSION_ARG% 2^>^&1 ^| findstr /r "[0-9][0-9]*\.[0-9][0-9]*"') do set "DETECTED_VERSION=%%v"
if "%DETECTED_VERSION%"=="" (
    echo [%STEP%/7] %TOOL_NAME% found ^(version unknown^).
    endlocal & exit /b 0
)

call :VERSION_COMPARE "%DETECTED_VERSION%" "%MIN_VERSION%"
if %errorlevel% equ 1 (
    echo [%STEP%/7] %TOOL_NAME% v%DETECTED_VERSION% ^< v%MIN_VERSION%, upgrading...
    if "%CI_SKIP%"=="1" (
        call :CHOCO_INSTALL --vs %CHOCO_PKG%
    ) else (
        call :CHOCO_INSTALL %CHOCO_PKG%
    )
    if !errorlevel! equ 0 (
        echo [%STEP%/7] %TOOL_NAME% upgraded.
        if "%CI_SKIP%"=="1" call :LOCATE_MSBUILD
    ) else (
        echo WARNING: %TOOL_NAME% upgrade failed, using v%DETECTED_VERSION%.
    )
) else (
    echo [%STEP%/7] %TOOL_NAME% v%DETECTED_VERSION% ^>= v%MIN_VERSION% ^(OK^)
)
endlocal & exit /b 0

:: ============================================
:: Helper: Simple version comparison
:: Handles "v3.20.0", "3.20", "17.0.6", etc.
:: ============================================
:VERSION_COMPARE
setlocal
set "V1=%~1"
set "V2=%~2"

:: Strip leading "v" or "V"
if /i "%V1:~0,1%"=="v" set "V1=%V1:~1%"
if /i "%V2:~0,1%"=="v" set "V2=%V2:~1%"

:: Strip leading non-digits (for "Java(TM) SE Runtime Environment 17.0.19" etc.)
set "CLEAN1="
for /f "tokens=*" %%a in ('echo !V1! ^| findstr /r "[0-9][0-9]*\.[0-9][0-9]*"') do set "CLEAN1=%%a"
if not "%CLEAN1%"=="" set "V1=%CLEAN1%"

for /f "tokens=1-3 delims=." %%a in ("%V1%") do set "A1=%%a" & set "A2=%%b" & set "A3=%%c"
for /f "tokens=1-3 delims=." %%a in ("%V2%") do set "B1=%%a" & set "B2=%%b" & set "B3=%%c"
:: Strip non-numeric suffix from A3 (e.g. "0+1" -> "0")
for /f "delims=+-_" %%x in ("%A3%") do set "A3=%%x"
for /f "delims=+-_" %%x in ("%B3%") do set "B3=%%x"
if "%A1%"=="" set A1=0
if "%A2%"=="" set A2=0
if "%A3%"=="" set A3=0
if "%B1%"=="" set B1=0
if "%B2%"=="" set B2=0
if "%B3%"=="" set B3=0
if %A1% lss %B1% endlocal & exit /b 1
if %A1% gtr %B1% endlocal & exit /b 0
if %A2% lss %B2% endlocal & exit /b 1
if %A2% gtr %B2% endlocal & exit /b 0
if %A3% lss %B3% endlocal & exit /b 1
endlocal & exit /b 0

:: ============================================
:: MAIN: Preflight — install everything needed
:: ============================================
:MAIN

:: Step 1: Chocolatey
set STEP=1
call :ENSURE_CHOCO_PATH
if %errorlevel% neq 0 (
    echo [1/7] Chocolatey not found, installing...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
    if %errorlevel% neq 0 (
        echo ERROR: Failed to install Chocolatey. Check internet connection.
        exit /b 1
    )
    call :ENSURE_CHOCO_PATH
    echo [1/7] Chocolatey installed.
) else (
    echo [1/7] Chocolatey available.
)
call :SAFE_REFRESH_ENV

:: Step 2: CMake (>= 3.20)
set STEP=2
call :CHECK_TOOL "cmake" "3.20" "--version" "cmake" "1" "0"

:: Step 3: MSBuild + VS 2022 Build Tools — skip on CI (pre-installed)
:: Add ARM64 toolchain for cross-compilation
set STEP=3
call :CHECK_TOOL "msbuild" "17.0" "-version" ^
    "visualstudio2022buildtools --package-parameters \"--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --includeRecommended --quiet\"" ^
    "1" "1"

:: Step 4: Pandoc (>= 2.18)
set STEP=4
call :CHECK_TOOL "pandoc" "2.18" "--version" "pandoc" "1" "0"

:: Step 5: .NET SDK (>= 6.0)
set STEP=5
call :CHECK_TOOL "dotnet" "6.0" "--version" "dotnet-sdk" "1" "0"
where dotnet >nul 2>&1
if %errorlevel% equ 0 (
    if exist "src\dotnet\TolkDotNet.csproj" (
        echo [5/7] Running dotnet restore...
        dotnet restore "src\dotnet\TolkDotNet.csproj"
        if !errorlevel! equ 0 (
            echo [5/7] dotnet restore completed.
        ) else (
            echo WARNING: dotnet restore failed, build may use cached packages.
        )
    )
)

:: Step 6: Java (OpenJDK 17) — optional, JAR will be skipped if missing
set STEP=6
call :CHECK_TOOL "java" "11" "--version" "openjdk17" "0" "0"

:: Step 7: Ninja (>= 1.10) — optional
set STEP=7
call :CHECK_TOOL "ninja" "1.10" "--version" "ninja" "0" "0"

:: Final check: msbuild must be on PATH
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    call :LOCATE_MSBUILD
    if %errorlevel% neq 0 (
        echo.
        echo ERROR: MSBuild could not be found.
        echo Visual Studio 2022 Build Tools may not have installed correctly.
        echo Try running: "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe"
        exit /b 1
    )
)

echo.
echo ============================================
echo  [Preflight] All required tools are ready!
echo  Ready to build Tolk.
echo ============================================
echo.

:: ============================================
:: BUILD
:: ============================================

:: Verify MSBuild path
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    for /f "usebackq tokens=*" %%p in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2^>nul`) do (
        set "MSBUILD_EXE=%%p"
    )
    if defined MSBUILD_EXE (
        for %%D in ("%MSBUILD_EXE%\..") do set "PATH=%%~dpD;!PATH!"
    )
)

:: Determine build targets
:: If no arch flag was specified, build all three architectures
if %ARCH_SPECIFIED%==0 (
    set BUILD_X86=1
    set BUILD_X64=1
    set BUILD_ARM64=1
)

:: x86 build
if %BUILD_X86%==1 (
    echo ============================================
    echo  Building x86 (%BUILD_CONFIG%)
    echo ============================================
    echo [1/3] Configuring CMake x86...
    cmake -B build-x86 -A Win32 2>&1
    if %errorlevel% equ 0 (
        echo [2/3] Building x86...
        cmake --build build-x86 --config %BUILD_CONFIG% 2>&1
        if %errorlevel% equ 0 (
            echo [3/3] x86 build succeeded.
        ) else (
            echo ERROR: x86 build failed.
            set BUILD_X86=0
        )
    ) else (
        echo ERROR: x86 CMake configuration failed.
        set BUILD_X86=0
    )
)

:: x64 build
if %BUILD_X64%==1 (
    echo.
    echo ============================================
    echo  Building x64 (%BUILD_CONFIG%)
    echo ============================================
    echo [1/3] Configuring CMake x64...
    cmake -B build-x64 -A x64 2>&1
    if %errorlevel% equ 0 (
        echo [2/3] Building x64...
        cmake --build build-x64 --config %BUILD_CONFIG% 2>&1
        if %errorlevel% equ 0 (
            echo [3/3] x64 build succeeded.
        ) else (
            echo ERROR: x64 build failed.
            set BUILD_X64=0
        )
    ) else (
        echo ERROR: x64 CMake configuration failed.
        set BUILD_X64=0
    )
)

:: ARM64 build
if %BUILD_ARM64%==1 (
    echo.
    echo ============================================
    echo  Building ARM64 (%BUILD_CONFIG%)
    echo ============================================
    echo [1/3] Configuring CMake ARM64...
    cmake -B build-arm64 -A ARM64 2>&1
    if %errorlevel% equ 0 (
        echo [2/3] Building ARM64...
        cmake --build build-arm64 --config %BUILD_CONFIG% 2>&1
        if %errorlevel% equ 0 (
            echo [3/3] ARM64 build succeeded.
        ) else (
            echo WARNING: ARM64 build failed ^(toolchain may be missing^).
        )
    ) else (
        echo WARNING: ARM64 CMake configuration failed ^(toolchain not available^).
    )
)

:: ============================================
:: ASSEMBLE DISTRIBUTION
:: ============================================
echo.
echo ============================================
echo  Assembling distribution...
echo ============================================

if not exist "dist" mkdir dist

:: Copy x86 output
if %BUILD_X86%==1 (
    if exist "build-x86\dist\x86-Debug" (
        xcopy /E /I /Y "build-x86\dist\x86-Debug" "dist\x86\Debug"
        echo   x86 Debug copied.
    )
    if exist "build-x86\dist\x86-Release" (
        xcopy /E /I /Y "build-x86\dist\x86-Release" "dist\x86\Release"
        echo   x86 Release copied.
    )
)

:: Copy x64 output
if %BUILD_X64%==1 (
    if exist "build-x64\dist\x64-Debug" (
        xcopy /E /I /Y "build-x64\dist\x64-Debug" "dist\x64\Debug"
        echo   x64 Debug copied.
    )
    if exist "build-x64\dist\x64-Release" (
        xcopy /E /I /Y "build-x64\dist\x64-Release" "dist\x64\Release"
        echo   x64 Release copied.
    )
    if exist "build-x64\src\dotnet\publish\TolkDotNet.dll" (
        copy /Y "build-x64\src\dotnet\publish\TolkDotNet.dll" "dist\"
        echo   .NET wrapper copied.
    )
    if exist "build-x64\src\java\Tolk.jar" (
        copy /Y "build-x64\src\java\Tolk.jar" "dist\"
        echo   Java JAR copied.
    )
    if exist "build-x64\docs\README.html" (
        copy /Y "build-x64\docs\README.html" "dist\"
        echo   Documentation copied.
    )
)

:: Copy ARM64 output
if %BUILD_ARM64%==1 (
    if exist "build-arm64\dist\ARM64-Debug" (
        xcopy /E /I /Y "build-arm64\dist\ARM64-Debug" "dist\arm64\Debug"
        echo   ARM64 Debug copied.
    )
    if exist "build-arm64\dist\ARM64-Release" (
        xcopy /E /I /Y "build-arm64\dist\ARM64-Release" "dist\arm64\Release"
        echo   ARM64 Release copied.
    )
)

:: Copy license files
if exist "LICENSE.txt"   copy /Y "LICENSE.txt"   "dist\"
if exist "LICENSE-NVDA.txt" copy /Y "LICENSE-NVDA.txt" "dist\"

:: Copy source wrappers (architecture-independent)
for %%W in (Tolk.py Tolk.au3 Tolk.pb) do (
    if exist "src\python\%%W"   copy /Y "src\python\%%W"   "dist\"
    if exist "src\autoit\%%W"   copy /Y "src\autoit\%%W"   "dist\"
    if exist "src\purebasic\%%W" copy /Y "src\purebasic\%%W" "dist\"
)

echo.
echo ============================================
echo  Build complete!
echo  Output: dist\
echo ============================================

:: ---------- Verify build results ----------
set BUILD_FAILED=0
set BUILD_ANY=0

if %BUILD_X86%==1 (
    set BUILD_ANY=1
    if not exist "dist\x86\Debug\Tolk.dll" if not exist "dist\x86\Release\Tolk.dll" (
        echo WARNING: x86 build marked as success but no DLL found in dist.
        set BUILD_FAILED=1
    )
)
if %BUILD_X64%==1 (
    set BUILD_ANY=1
    if not exist "dist\x64\Debug\Tolk.dll" if not exist "dist\x64\Release\Tolk.dll" (
        echo WARNING: x64 build marked as success but no DLL found in dist.
        set BUILD_FAILED=1
    )
)
if %BUILD_ARM64%==1 (
    set BUILD_ANY=1
    if not exist "dist\arm64\Debug\Tolk.dll" if not exist "dist\arm64\Release\Tolk.dll" (
        echo WARNING: ARM64 build marked as success but no DLL found in dist.
        set BUILD_FAILED=1
    )
)

if %BUILD_ANY%==0 (
    echo.
    echo FATAL: All builds failed. No output was produced.
    echo.
    exit /b 1
)

if %BUILD_FAILED%==1 (
    echo.
    echo WARNING: Some builds did not produce expected output.
    echo.
)

exit /b 0