@echo off
setlocal enabledelayedexpansion

:: ============================================
::  Tolk Build Script
::  x86 + x64 + ARM64 (Debug + Release)
::  Usage: build.bat [debug|release] [--clean]
:: ============================================

:: ---------- Parse arguments ----------
set BUILD_CONFIG=Release
set DO_CLEAN=0
:PARSE_ARGS
if "%~1"=="" goto :START
if /i "%~1"=="debug"   set BUILD_CONFIG=Debug
if /i "%~1"=="release" set BUILD_CONFIG=Release
if /i "%~1"=="--clean" set DO_CLEAN=1
shift
goto :PARSE_ARGS

:START
:: Detect CI environment
set IS_CI=0
if defined CI             set IS_CI=1
if defined APPVEYOR       set IS_CI=1
if defined GITHUB_ACTIONS set IS_CI=1
:: CI always does a clean build (no stale cache)
if %IS_CI%==1 set DO_CLEAN=1

echo ============================================
echo  Tolk Build Script
echo  Config: %BUILD_CONFIG%  CI: %IS_CI%  Clean: %DO_CLEAN%
echo ============================================

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
    "%ALLUSERSPROFILE%\chocolatey\bin\RefreshEnv.cmd"
    "%ProgramData%\chocolatey\bin\RefreshEnv.cmd"
) do (
    if exist %%P (
        call %%P >nul 2>&1
        exit /b 0
    )
)
exit /b 0

:: ============================================
:: Helper: Chocolatey install with retry
:: ============================================
:CHOCO_INSTALL
setlocal
set /a TRY=0
:CHOCO_RETRY
choco install %* -y --no-progress --limit-output --allow-downgrade >nul 2>&1
set RC=%errorlevel%
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

:: On CI, skip VS/MSBuild installation (VS 2022 is pre-installed)
if %IS_CI%==1 if "%CI_SKIP%"=="1" (
    echo [%STEP%/7] %TOOL_NAME%: using pre-installed Visual Studio 2022
    endlocal & exit /b 0
)

:: Check if tool exists
where "%TOOL_NAME%" >nul 2>&1
if %errorlevel% neq 0 (
    echo [%STEP%/7] %TOOL_NAME% not found, installing...
    call :CHOCO_INSTALL %CHOCO_PKG%
    if !errorlevel! equ 0 (
        echo [%STEP%/7] %TOOL_NAME% installed.
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

:: Check version
for /f "tokens=*" %%v in ('%TOOL_NAME% %VERSION_ARG% 2^>^&1 ^| findstr /r "[0-9][0-9]*\.[0-9][0-9]*"') do set "DETECTED_VERSION=%%v"
if "%DETECTED_VERSION%"=="" (
    echo [%STEP%/7] %TOOL_NAME% found ^(version unknown^).
    endlocal & exit /b 0
)

call :VERSION_COMPARE "%DETECTED_VERSION%" "%MIN_VERSION%"
if %errorlevel% equ 1 (
    echo [%STEP%/7] %TOOL_NAME% v%DETECTED_VERSION% ^< v%MIN_VERSION%, upgrading...
    call :CHOCO_INSTALL %CHOCO_PKG%
    if !errorlevel! equ 0 (
        echo [%STEP%/7] %TOOL_NAME% upgraded.
    ) else (
        echo WARNING: %TOOL_NAME% upgrade failed, using v%DETECTED_VERSION%.
    )
) else (
    echo [%STEP%/7] %TOOL_NAME% v%DETECTED_VERSION% ^>= v%MIN_VERSION% ^(OK^)
)
endlocal & exit /b 0

:: ============================================
:: Helper: Simple version comparison
:: ============================================
:VERSION_COMPARE
setlocal
set "V1=%~1"
set "V2=%~2"
:: Strip leading non-digits
for /f "tokens=1-3 delims=." %%a in ("%V1%") do set "A1=%%a" & set "A2=%%b" & set "A3=%%c"
for /f "tokens=1-3 delims=." %%a in ("%V2%") do set "B1=%%a" & set "B2=%%b" & set "B3=%%c"
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
:: MAIN: Preflight tool checks
:: ============================================
:MAIN

:: Step 1: Chocolatey
set STEP=1
where choco >nul 2>&1
if %errorlevel% neq 0 (
    echo [1/7] Chocolatey not found, installing...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" >nul 2>&1
    set "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
    echo [1/7] Chocolatey installed.
) else (
    echo [1/7] Chocolatey available.
)
call :SAFE_REFRESH_ENV

:: Step 2: CMake (>= 3.20)
set STEP=2
call :CHECK_TOOL "cmake" "3.20" "--version" "cmake" "1" "0"

:: Step 3: MSBuild (VS 2022 Build Tools) — skip on CI
set STEP=3
call :CHECK_TOOL "msbuild" "17.0" "-version" "visualstudio2022buildtools --package-parameters \"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended\"" "1" "1"

:: Step 4: Pandoc (>= 2.18)
set STEP=4
call :CHECK_TOOL "pandoc" "2.18" "--version" "pandoc" "1" "0"

:: Step 5: .NET SDK (>= 6.0)
set STEP=5
call :CHECK_TOOL "dotnet" "6.0" "--version" "dotnet-sdk" "1" "0"
:: Restore NuGet packages (no-op for TolkDotNet which has no external deps,
:: but required for the --no-restore flag in CMake to work)
where dotnet >nul 2>&1
if %errorlevel% equ 0 (
    if exist "src\dotnet\TolkDotNet.csproj" (
        echo [5/7] Running dotnet restore...
        dotnet restore "src\dotnet\TolkDotNet.csproj" >nul 2>&1
        if !errorlevel! equ 0 (
            echo [5/7] dotnet restore completed.
        ) else (
            echo WARNING: dotnet restore failed, build may use cached packages.
        )
    )
)

:: Step 6: Java (OpenJDK 17)
set STEP=6
call :CHECK_TOOL "java" "11" "--version" "openjdk17" "0" "0"

:: Step 7: Ninja (>= 1.10) — optional
set STEP=7
call :CHECK_TOOL "ninja" "1.10" "--version" "ninja" "0" "0"

echo.
echo [Preflight] All required build tools are ready!
echo ============================================
echo.

:: ============================================
:: BUILD
:: ============================================

:: Determine build targets
set BUILD_X86=1
set BUILD_X64=1
set BUILD_ARM64=1

:: ARM64: only attempt if VS 2022 ARM64 toolchain is present
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo WARNING: MSBuild not found, skipping ARM64 build.
    set BUILD_ARM64=0
)

:: x86 build
if %BUILD_X86%==1 (
    echo ============================================
    echo  Building x86 (%BUILD_CONFIG%)
    echo ============================================
    echo [1/3] Configuring CMake x86...
    cmake -B build-x86 -A Win32 -DCMAKE_BUILD_TYPE=%BUILD_CONFIG% 2>&1
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
    cmake -B build-x64 -A x64 -DCMAKE_BUILD_TYPE=%BUILD_CONFIG% 2>&1
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
    cmake -B build-arm64 -A ARM64 -DCMAKE_BUILD_TYPE=%BUILD_CONFIG% 2>&1
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

:: Create dist directories
if not exist "dist" mkdir dist

:: Copy x86 output
if %BUILD_X86%==1 (
    if exist "build-x86\dist\x86-Debug" (
        xcopy /E /I /Y "build-x86\dist\x86-Debug" "dist\x86\Debug" >nul
        echo   x86 Debug copied.
    )
    if exist "build-x86\dist\x86-Release" (
        xcopy /E /I /Y "build-x86\dist\x86-Release" "dist\x86\Release" >nul
        echo   x86 Release copied.
    )
)

:: Copy x64 output
if %BUILD_X64%==1 (
    if exist "build-x64\dist\x64-Debug" (
        xcopy /E /I /Y "build-x64\dist\x64-Debug" "dist\x64\Debug" >nul
        echo   x64 Debug copied.
    )
    if exist "build-x64\dist\x64-Release" (
        xcopy /E /I /Y "build-x64\dist\x64-Release" "dist\x64\Release" >nul
        echo   x64 Release copied.
    )
    :: Copy .NET wrapper (architecture-independent, built once with x64)
    if exist "build-x64\src\dotnet\publish\TolkDotNet.dll" (
        copy /Y "build-x64\src\dotnet\publish\TolkDotNet.dll" "dist\" >nul
        echo   .NET wrapper copied.
    )
    :: Copy Java JAR
    if exist "build-x64\src\java\Tolk.jar" (
        copy /Y "build-x64\src\java\Tolk.jar" "dist\" >nul
        echo   Java JAR copied.
    )
    :: Copy documentation
    if exist "build-x64\docs\README.html" (
        copy /Y "build-x64\docs\README.html" "dist\" >nul
        echo   Documentation copied.
    )
)

:: Copy ARM64 output
if %BUILD_ARM64%==1 (
    if exist "build-arm64\dist\ARM64-Debug" (
        xcopy /E /I /Y "build-arm64\dist\ARM64-Debug" "dist\arm64\Debug" >nul
        echo   ARM64 Debug copied.
    )
    if exist "build-arm64\dist\ARM64-Release" (
        xcopy /E /I /Y "build-arm64\dist\ARM64-Release" "dist\arm64\Release" >nul
        echo   ARM64 Release copied.
    )
)

:: Copy license files
if exist "LICENSE.txt"   copy /Y "LICENSE.txt"   "dist\" >nul
if exist "LICENSE-NVDA.txt" copy /Y "LICENSE-NVDA.txt" "dist\" >nul

:: Copy source wrappers (language bindings, architecture-independent)
for %%W in (Tolk.py Tolk.au3 Tolk.pb) do (
    if exist "src\python\%%W"   copy /Y "src\python\%%W"   "dist\" >nul 2>&1
    if exist "src\autoit\%%W"   copy /Y "src\autoit\%%W"   "dist\" >nul 2>&1
    if exist "src\purebasic\%%W" copy /Y "src\purebasic\%%W" "dist\" >nul 2>&1
)

echo.
echo ============================================
echo  Build complete!
echo  Output: dist\
echo ============================================