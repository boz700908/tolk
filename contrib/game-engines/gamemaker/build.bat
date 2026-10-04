@echo off
REM Builds the GameMaker shim TolkGml.dll.
REM Run from a Visual Studio "Native Tools Command Prompt" for the matching
REM architecture, then copy TolkGml.dll, Tolk.dll and the screen reader client
REM modules into the game directory.
setlocal
set ARCH=%1
if "%ARCH%"=="" set ARCH=x64

set ROOT=%~dp0..\..\..\..
if not exist "%ROOT%\dist\%ARCH%\Release\Tolk.lib" (
  echo Tolk.lib for %ARCH% not found. Build it first:
  echo     build.bat release --%ARCH%
  exit /b 1
)

cl /nologo /LD /O2 /W4 /I"%ROOT%\src" "%~dp0source\tolk_gml.c" ^
  /Fe:"%~dp0TolkGml.dll" ^
  /link /LIBPATH:"%ROOT%\dist\%ARCH%\Release" Tolk.lib
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)
echo Built %~dp0TolkGml.dll
endlocal