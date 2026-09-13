@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "ROOT_DIR=%%~fI"
set "BUILD_DIR=%SCRIPT_DIR%build\bin"
set "INSTALLER_SRC=%SCRIPT_DIR%build\install.cpp"
set "UNINSTALLER_SRC=%SCRIPT_DIR%build\uninstall.cpp"
set "INSTALL_BIN=%BUILD_DIR%\INSTALL.exe"
set "UNINSTALL_BIN=%BUILD_DIR%\UNINSTALL.exe"

echo ===== ign-lib installer =====

where g++ >nul 2>&1
if errorlevel 1 (
    echo [!] Error: g++ compiler is not installed or not in PATH.
    exit /b 1
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

call :compile "%INSTALLER_SRC%" "%INSTALL_BIN%" "INSTALL"
if errorlevel 1 exit /b 1

call :compile "%UNINSTALLER_SRC%" "%UNINSTALL_BIN%" "UNINSTALL"
if errorlevel 1 exit /b 1

echo [*] Running installer...
"%INSTALL_BIN%" --source "%ROOT_DIR%" %*
exit /b %errorlevel%

:compile
echo [*] Compiling %~3...
g++ -std=c++17 -Wall -Wextra -I"%ROOT_DIR%" "%~1" -o "%~2"
if %errorlevel% equ 0 exit /b 0

echo [*] Retrying %~3 with -lstdc++fs...
g++ -std=c++17 -Wall -Wextra -I"%ROOT_DIR%" "%~1" -o "%~2" -lstdc++fs
exit /b %errorlevel%
