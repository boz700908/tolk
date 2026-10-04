@echo off
setlocal enabledelayedexpansion

:: ============================================================
::  Tolk build script
::  Builds Tolk.dll and the language wrappers for x86, x64 and
::  ARM64, in Debug and/or Release configuration.
::
::  Usage:
::    build.bat [debug|release|both] [--x86] [--x64] [--arm64] [--arm64ec]
::              [--clean] [--ci] [--no-bootstrap] [--bootstrap] [--help]
::
::    debug|release|both   configuration to build (default: both)
::    --x86/--x64/--arm64/--arm64ec
::                         limit the build to the given architecture(s)
::    --clean              delete build-*/ and dist/ before building
::    --ci                 force non-interactive CI mode (never installs)
::    --no-bootstrap       never install missing build tools
::    --bootstrap          install missing tools even in CI mode
::    --help               show this help
::
::  On CI (GITHUB_ACTIONS, APPVEYOR, TF_BUILD, BUILD_BUILDID, CI=true)
::  the script runs in CI mode automatically and always builds clean.
::  On a clean Windows machine it installs missing build tools through
::  Chocolatey, which requires Administrator privileges.
::
::  Exit code: 0 if every requested build succeeded, 1 otherwise.
:: ============================================================

:: ---------- Defaults ----------
set "CONFIG=Both"
set "DO_CLEAN=0"
set "FORCE_CI=0"
set "NO_BOOTSTRAP=0"
set "FORCE_BOOTSTRAP=0"
set "BUILD_X86=0"
set "BUILD_X64=0"
set "BUILD_ARM64=0"
set "BUILD_ARM64EC=0"
set "ARM64_REQ=0"
set "ARM64EC_REQ=0"
set "ARCH_SPECIFIED=0"
set "FAIL_COUNT=0"
set "OK_COUNT=0"
set "ASM_FAIL=0"

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

call :PARSE_ARGS %*
set "PA=!errorlevel!"
if "!PA!"=="3" exit /b 0
if not "!PA!"=="0" exit /b 1

call :PREPARE_ENV
if errorlevel 1 exit /b 1

call :BUILD_ALL
set "RC=!errorlevel!"

call :ASSEMBLE
if "!RC!"=="0" set "RC=!errorlevel!"

call :SUMMARY
exit /b !RC!

:: ============================================================
:: Argument parsing
:: ============================================================
:PARSE_ARGS
if "%~1"=="" exit /b 0
set "ARG=%~1"
set "KNOWN=0"
if /i "!ARG!"=="debug"    (set "CONFIG=Debug" & set "KNOWN=1")
if /i "!ARG!"=="release"  (set "CONFIG=Release" & set "KNOWN=1")
if /i "!ARG!"=="both"     (set "CONFIG=Both" & set "KNOWN=1")
if /i "!ARG!"=="--debug"   (set "CONFIG=Debug" & set "KNOWN=1")
if /i "!ARG!"=="--release" (set "CONFIG=Release" & set "KNOWN=1")
if /i "!ARG!"=="--both"    (set "CONFIG=Both" & set "KNOWN=1")
if /i "!ARG!"=="--x86"    (set "ARCH_SPECIFIED=1" & set "BUILD_X86=1" & set "KNOWN=1")
if /i "!ARG!"=="--x64"    (set "ARCH_SPECIFIED=1" & set "BUILD_X64=1" & set "KNOWN=1")
if /i "!ARG!"=="--arm64"  (set "ARCH_SPECIFIED=1" & set "BUILD_ARM64=1" & set "ARM64_REQ=1" & set "KNOWN=1")
if /i "!ARG!"=="--arm64ec" (set "ARCH_SPECIFIED=1" & set "BUILD_ARM64EC=1" & set "ARM64EC_REQ=1" & set "KNOWN=1")
if /i "!ARG!"=="--clean"  (set "DO_CLEAN=1" & set "KNOWN=1")
if /i "!ARG!"=="--ci"     (set "FORCE_CI=1" & set "KNOWN=1")
if /i "!ARG!"=="--no-bootstrap" (set "NO_BOOTSTRAP=1" & set "KNOWN=1")
if /i "!ARG!"=="--bootstrap"    (set "FORCE_BOOTSTRAP=1" & set "KNOWN=1")
if /i "!ARG!"=="--help" (call :USAGE & exit /b 3)
if /i "!ARG!"=="-h"     (call :USAGE & exit /b 3)
if "!KNOWN!"=="0" echo WARNING: ignoring unknown argument "!ARG!"
shift
goto :PARSE_ARGS

:USAGE
echo Tolk build script
echo.
echo Usage: build.bat [debug^|release^|both] [--x86] [--x64] [--arm64] [--arm64ec]
echo                  [--clean] [--ci] [--no-bootstrap] [--help]
echo.
echo   debug^|release^|both   configuration to build (default: both)
echo   --x86/--x64/--arm64/--arm64ec
echo                        limit the build to the given architecture^(s^)
echo   --clean              delete build-*/ and dist/ before building
echo   --ci                 force non-interactive CI mode
echo   --no-bootstrap       never install missing build tools
echo   --bootstrap          install missing tools even in CI mode
echo   --help               show this help
exit /b 0

:: ============================================================
:: Environment preparation
:: ============================================================
:PREPARE_ENV
:: Detect CI
set "IS_CI=0"
if defined GITHUB_ACTIONS set "IS_CI=1"
if defined APPVEYOR set "IS_CI=1"
if defined TF_BUILD set "IS_CI=1"
if defined BUILD_BUILDID set "IS_CI=1"
if defined CI if /i not "%CI%"=="false" set "IS_CI=1"
if "%FORCE_CI%"=="1" set "IS_CI=1"
if "%IS_CI%"=="1" set "DO_CLEAN=1"

:: Resolve configuration and architecture lists
if /i "%CONFIG%"=="both" (set "CONFIG_LIST=Debug Release") else (set "CONFIG_LIST=%CONFIG%")
if "%ARCH_SPECIFIED%"=="0" (
  set "BUILD_X86=1"
  set "BUILD_X64=1"
  set "BUILD_ARM64=1"
  set "BUILD_ARM64EC=1"
)

set "ARCH_LIST="
if "!BUILD_X86!"=="1"   set "ARCH_LIST=!ARCH_LIST! x86"
if "!BUILD_X64!"=="1"   set "ARCH_LIST=!ARCH_LIST! x64"
if "!BUILD_ARM64!"=="1" set "ARCH_LIST=!ARCH_LIST! arm64"
if "!BUILD_ARM64EC!"=="1" set "ARCH_LIST=!ARCH_LIST! arm64ec"

echo ============================================================
echo  Tolk build
echo  Configs: !CONFIG_LIST!   Archs:!ARCH_LIST!   CI: !IS_CI!   Clean: !DO_CLEAN!
echo ============================================================

if "%DO_CLEAN%"=="1" (
  echo [Clean] Removing previous build output...
  for %%D in (build-x86 build-x64 build-arm64 build-arm64ec dist) do (
    if exist "%%D" rmdir /s /q "%%D"
  )
)

:: Locate toolchain
call :FIND_CMAKE
call :FIND_VS

:: ARM64 is optional: skip it when the toolchain is not installed
if "!BUILD_ARM64!"=="1" if "!VS_FOUND!"=="1" if not "!VS_ARM64!"=="1" (
  if "!ARM64_REQ!"=="1" (
    echo ERROR: ARM64 was requested but the ARM64 C++ build tools are not installed.
    exit /b 1
  )
  echo WARNING: ARM64 C++ build tools are not installed, skipping ARM64.
  echo          Install "MSVC v143 - VS 2022 C++ ARM64 build tools" to enable it.
  set "BUILD_ARM64=0"
)

:: ARM64EC is optional as well: skip it when the toolchain is not installed
if "!BUILD_ARM64EC!"=="1" if "!VS_FOUND!"=="1" if not "!VS_ARM64EC!"=="1" (
  if "!ARM64EC_REQ!"=="1" (
    echo ERROR: ARM64EC was requested but the ARM64EC C++ build tools are not installed.
    exit /b 1
  )
  echo WARNING: ARM64EC C++ build tools are not installed, skipping ARM64EC.
  echo          Install "MSVC v143 - VS 2022 C++ ARM64EC build tools" to enable it.
  set "BUILD_ARM64EC=0"
)

:: Bootstrap missing required tools (local builds only)
set "NEED_BOOTSTRAP=0"
if not defined CMAKE_EXE set "NEED_BOOTSTRAP=1"
if not "!VS_FOUND!"=="1" set "NEED_BOOTSTRAP=1"
set "ALLOW_BOOTSTRAP=1"
if "%NO_BOOTSTRAP%"=="1" set "ALLOW_BOOTSTRAP=0"
if "%IS_CI%"=="1" if not "%FORCE_BOOTSTRAP%"=="1" set "ALLOW_BOOTSTRAP=0"
if "%NEED_BOOTSTRAP%"=="1" (
  if "%ALLOW_BOOTSTRAP%"=="0" (
    echo ERROR: required build tools ^(CMake / Visual Studio C++^) are missing.
    if "%IS_CI%"=="1" (
      echo        The CI image is expected to provide CMake and the Visual Studio C++ build tools.
    ) else (
      echo        Run without --no-bootstrap to install them.
    )
    exit /b 1
  )
  call :REQUIRE_ADMIN
  if errorlevel 1 exit /b 1
  call :BOOTSTRAP
)

if not defined CMAKE_EXE (
  echo ERROR: CMake could not be found or installed.
  exit /b 1
)
if not "!VS_FOUND!"=="1" (
  echo ERROR: Visual Studio C++ build tools could not be found or installed.
  exit /b 1
)

:: Configure COM-free optional tools (only used for wrappers/docs)
set "DOTNET_EXE="
for /f "delims=" %%i in ('where dotnet 2^>nul') do if not defined DOTNET_EXE set "DOTNET_EXE=%%i"
if defined DOTNET_EXE if exist "src\dotnet\TolkDotNet.csproj" (
  echo [Restore] dotnet restore src\dotnet\TolkDotNet.csproj
  "%DOTNET_EXE%" restore "src\dotnet\TolkDotNet.csproj" >nul
  if errorlevel 1 echo WARNING: dotnet restore failed, the .NET wrapper may not build.
)

echo [Tools] CMAKE=!CMAKE_EXE!
exit /b 0

:: ============================================================
:: Build every requested architecture/configuration combination
:: ============================================================
:BUILD_ALL
for %%C in (%CONFIG_LIST%) do (
  if "!BUILD_X86!"=="1"   call :BUILD_ONE x86 Win32 %%C
  if "!BUILD_X64!"=="1"   call :BUILD_ONE x64 x64 %%C
  if "!BUILD_ARM64!"=="1" call :BUILD_ONE arm64 ARM64 %%C
  if "!BUILD_ARM64EC!"=="1" call :BUILD_ONE arm64ec ARM64EC %%C
)
if !FAIL_COUNT! gtr 0 exit /b 1
exit /b 0

:BUILD_ONE
set "ARCH=%~1"
set "CM_ARCH=%~2"
set "CFG=%~3"
set "BDIR=build-%~1"
echo.
echo ============================================================
echo  Building %ARCH% %CFG%
echo ============================================================
"%CMAKE_EXE%" -B "%BDIR%" -A "%CM_ARCH%"
if errorlevel 1 (
  echo ERROR: CMake configuration failed for %ARCH% %CFG%
  set /a FAIL_COUNT+=1
  exit /b 1
)
"%CMAKE_EXE%" --build "%BDIR%" --config "%CFG%" --parallel
if errorlevel 1 (
  echo ERROR: compilation failed for %ARCH% %CFG%
  set /a FAIL_COUNT+=1
  exit /b 1
)
set /a OK_COUNT+=1
exit /b 0

:: ============================================================
:: Assemble the dist/ directory
:: ============================================================
:ASSEMBLE
if not exist "dist" mkdir "dist"
for %%C in (%CONFIG_LIST%) do (
  if "!BUILD_X86!"=="1"   call :COPY_ARCH x86 %%C
  if "!BUILD_X64!"=="1"   call :COPY_ARCH x64 %%C
  if "!BUILD_ARM64!"=="1" call :COPY_ARCH arm64 %%C
  if "!BUILD_ARM64EC!"=="1" call :COPY_ARCH arm64ec %%C
)
call :COPY_SHARED
call :COPY_LICENSES
call :WRITE_DEBUG_FEATURES
if "!ASM_FAIL!"=="1" exit /b 1
exit /b 0

:COPY_ARCH
set "ARCH=%~1"
set "CFG=%~2"
set "SRC=build-%ARCH%\dist\%ARCH%-%CFG%"
set "DST=dist\%ARCH%\%CFG%"
if not exist "%SRC%" (
  echo WARNING: no build output found for %ARCH% %CFG%
  set "ASM_FAIL=1"
  exit /b 1
)
if not exist "%DST%" mkdir "%DST%"
xcopy /E /I /Y /Q "%SRC%\*" "%DST%\" >nul
if exist "build-%ARCH%\src\%CFG%\Tolk.pdb" copy /Y "build-%ARCH%\src\%CFG%\Tolk.pdb" "%DST%\" >nul
if exist "build-%ARCH%\src\%CFG%\Tolk.exp" copy /Y "build-%ARCH%\src\%CFG%\Tolk.exp" "%DST%\" >nul
if not exist "%DST%\Tolk.dll" (
  echo WARNING: Tolk.dll is missing from %DST%
  set "ASM_FAIL=1"
  exit /b 1
)
echo   [dist] %DST%
exit /b 0

:: Language wrappers are identical for every architecture, so collect
:: them once under dist\wrappers and drop the per-architecture copies.
:COPY_SHARED
set "WSRC="
for %%C in (Release Debug) do (
  for %%A in (x64 x86 arm64 arm64ec) do (
    if not defined WSRC if exist "dist\%%A\%%C\python\Tolk.py" set "WSRC=dist\%%A\%%C"
  )
)
if defined WSRC (
  for %%W in (python autoit purebasic) do (
    if exist "!WSRC!\%%W" (
      if not exist "dist\wrappers\%%W" mkdir "dist\wrappers\%%W"
      xcopy /E /I /Y /Q "!WSRC!\%%W\*" "dist\wrappers\%%W\" >nul
    )
  )
  for %%A in (x86 x64 arm64 arm64ec) do (
    for %%D in (Debug Release) do (
      for %%W in (python autoit purebasic) do (
        if exist "dist\%%A\%%D\%%W" rmdir /s /q "dist\%%A\%%D\%%W"
      )
    )
  )
)

:: .NET wrapper
set "DLL="
for %%C in (Release Debug) do (
  for %%A in (x64 x86 arm64 arm64ec) do (
    if not defined DLL if exist "build-%%A\src\dotnet\publish\TolkDotNet.dll" set "DLL=build-%%A\src\dotnet\publish\TolkDotNet.dll"
  )
)
if defined DLL (
  if not exist "dist\wrappers\dotnet" mkdir "dist\wrappers\dotnet"
  if not exist "dist\dotnet" mkdir "dist\dotnet"
  copy /Y "!DLL!" "dist\wrappers\dotnet\TolkDotNet.dll" >nul
  copy /Y "!DLL!" "dist\dotnet\TolkDotNet.dll" >nul
  copy /Y "!DLL!" "dist\TolkDotNet.dll" >nul
)

:: Java wrapper
set "JAR="
for %%C in (Release Debug) do (
  for %%A in (x64 x86 arm64 arm64ec) do (
    if not defined JAR if exist "build-%%A\src\java\Tolk.jar" set "JAR=build-%%A\src\java\Tolk.jar"
  )
)
if defined JAR (
  if not exist "dist\wrappers\java" mkdir "dist\wrappers\java"
  if not exist "dist\java" mkdir "dist\java"
  copy /Y "!JAR!" "dist\wrappers\java\Tolk.jar" >nul
  copy /Y "!JAR!" "dist\java\Tolk.jar" >nul
  copy /Y "!JAR!" "dist\Tolk.jar" >nul
)

:: Documentation
set "HTML="
for %%C in (Release Debug) do (
  for %%A in (x64 x86 arm64 arm64ec) do (
    if not defined HTML if exist "build-%%A\docs\README.html" set "HTML=build-%%A\docs\README.html"
  )
)
if defined HTML (
  if not exist "dist\docs" mkdir "dist\docs"
  copy /Y "!HTML!" "dist\docs\README.html" >nul
  copy /Y "!HTML!" "dist\README.html" >nul
)
exit /b 0

:COPY_LICENSES
if exist "LICENSE.txt"     copy /Y "LICENSE.txt"     "dist\LICENSE.txt" >nul
if exist "LICENSE-NVDA.txt" copy /Y "LICENSE-NVDA.txt" "dist\LICENSE-NVDA.txt" >nul
exit /b 0

:WRITE_DEBUG_FEATURES
(
  echo Debug Build Features:
  echo =====================
  echo.
  echo 1. Tolk_Debug.log written to the calling process working directory
  echo 2. ERR ^(red^) and WRN ^(yellow^) messages printed to the console
  echo 3. All logs sent to Windows OutputDebugString ^(view with DebugView^)
  echo 4. Full PDB debug symbols included
  echo 5. Runtime error checking enabled ^(RTC1 + RTCsu^)
  echo 6. No optimization for easier debugging
  echo.
  echo Log file location: Tolk_Debug.log ^(in your application working directory^)
) > "dist\DEBUG_FEATURES.txt"
exit /b 0

:: ============================================================
:: Summary
:: ============================================================
:SUMMARY
echo.
echo ============================================================
echo  Build finished: !OK_COUNT! succeeded, !FAIL_COUNT! failed
echo  Output directory: %SCRIPT_DIR%dist
echo ============================================================
if !FAIL_COUNT! gtr 0 (
  echo Result: FAILED
) else (
  echo Result: SUCCESS
)
exit /b 0

:: ============================================================
:: Tool discovery / bootstrap helpers
:: ============================================================
:FIND_CMAKE
set "CMAKE_EXE="
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%i"
:: VS ships CMake under Common7\IDE\CommonExtensions\Microsoft\CMake.
:: Ask vswhere for the installation path (no embedded quotes in the
:: command, which cmd's "for /f" cannot parse) and append the rest.
if not defined CMAKE_EXE (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    set "VSINSTALL="
    for /f "usebackq delims=" %%p in (`"!VSWHERE!" -latest -products * -property installationPath`) do set "VSINSTALL=%%p"
    if defined VSINSTALL if exist "!VSINSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
      set "CMAKE_EXE=!VSINSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
  )
)
if not defined CMAKE_EXE for %%P in (
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) do if not defined CMAKE_EXE if exist %%P set "CMAKE_EXE=%%~P"
if defined CMAKE_EXE for %%D in ("!CMAKE_EXE!") do set "PATH=%%~dpD;!PATH!"
exit /b 0

:FIND_VS
set "VS_FOUND=0"
set "VS_ARM64=0"
set "VS_ARM64EC=0"
set "VS_PATH="
set "VS_ARM64_PATH="
set "VS_ARM64EC_PATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" exit /b 0
for /f "usebackq delims=" %%p in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VS_PATH=%%p"
if defined VS_PATH set "VS_FOUND=1"
for /f "usebackq delims=" %%p in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath 2^>nul`) do set "VS_ARM64_PATH=%%p"
if defined VS_ARM64_PATH set "VS_ARM64=1"
for /f "usebackq delims=" %%p in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64EC -property installationPath 2^>nul`) do set "VS_ARM64EC_PATH=%%p"
if defined VS_ARM64EC_PATH set "VS_ARM64EC=1"
exit /b 0

:REQUIRE_ADMIN
net session >nul 2>&1
if errorlevel 1 (
  echo.
  echo ERROR: Administrator privileges are required to install the missing build tools.
  echo        Right-click the command prompt and choose "Run as administrator",
  echo        or install CMake and the Visual Studio C++ build tools manually.
  echo.
  exit /b 1
)
exit /b 0

:BOOTSTRAP
echo [Bootstrap] Installing missing build tools through Chocolatey...
call :ENSURE_CHOCO
if errorlevel 1 exit /b 1
if not defined CMAKE_EXE call :CHOCO_INSTALL cmake
if not "!VS_FOUND!"=="1" (
  call :CHOCO_INSTALL visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --add Microsoft.VisualStudio.Component.VC.Tools.ARM64EC --includeRecommended --quiet"
)
:: Optional tools: only used for the language wrappers and documentation.
call :ENSURE_OPTIONAL_TOOL dotnet dotnet-sdk
call :ENSURE_OPTIONAL_TOOL pandoc pandoc
call :ENSURE_OPTIONAL_TOOL java openjdk17
call :FIND_CMAKE
call :FIND_VS
exit /b 0

:ENSURE_OPTIONAL_TOOL
where %~1 >nul 2>&1 && exit /b 0
echo [Bootstrap] Installing optional tool: %~2
call :CHOCO_INSTALL %~2
exit /b 0

:REFRESH_ENV
if exist "%ProgramData%\chocolatey\bin\RefreshEnv.cmd" (
  call "%ProgramData%\chocolatey\bin\RefreshEnv.cmd" >nul 2>&1
  exit /b 0
)
for /f "usebackq tokens=2,*" %%a in (`reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v Path 2^>nul`) do set "SYSPATH=%%b"
for /f "usebackq tokens=2,*" %%a in (`reg query "HKCU\Environment" /v Path 2^>nul`) do set "USERPATH=%%b"
if defined SYSPATH set "PATH=!SYSPATH!"
if defined USERPATH set "PATH=!PATH!;!USERPATH!"
exit /b 0

:ENSURE_CHOCO
where choco >nul 2>&1 && exit /b 0
if exist "%ProgramData%\chocolatey\bin\choco.exe" (
  set "PATH=%ProgramData%\chocolatey\bin;!PATH!"
  exit /b 0
)
echo [Bootstrap] Installing Chocolatey...
powershell -NoProfile -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
if errorlevel 1 (
  echo ERROR: failed to install Chocolatey.
  exit /b 1
)
if exist "%ProgramData%\chocolatey\bin" set "PATH=%ProgramData%\chocolatey\bin;!PATH!"
exit /b 0

:CHOCO_INSTALL
call :ENSURE_CHOCO
if errorlevel 1 exit /b 1
echo [Bootstrap] choco install %* -y --no-progress --limit-output
choco install %* -y --no-progress --limit-output
if errorlevel 1 (
  echo ERROR: Chocolatey failed to install: %*
  exit /b 1
)
call :REFRESH_ENV
exit /b 0
