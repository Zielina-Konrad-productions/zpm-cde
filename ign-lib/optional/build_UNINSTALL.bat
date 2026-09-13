@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%SCRIPT_DIR%build\bin"
set "UNINSTALLER_SRC=%SCRIPT_DIR%build\uninstall.cpp"
set "UNINSTALL_BIN=%BUILD_DIR%\UNINSTALL.exe"

echo ===== ign-lib uninstaller =====

where g++ >nul 2>&1
if errorlevel 1 (
    echo [!] Error: g++ compiler is not installed or not in PATH.
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo [*] Compiling UNINSTALL...
g++ -std=c++17 -Wall -Wextra -I"%ROOT_DIR%" "%UNINSTALLER_SRC%" -o "%UNINSTALL_BIN%"
if %errorlevel% neq 0 (
    echo [*] Retrying UNINSTALL with -lstdc++fs...
    g++ -std=c++17 -Wall -Wextra -I"%ROOT_DIR%" "%UNINSTALLER_SRC%" -o "%UNINSTALL_BIN%" -lstdc++fs
)
if errorlevel 1 exit /b 1

echo [*] Running uninstaller...
"%UNINSTALL_BIN%" %*
exit /b %errorlevel%
