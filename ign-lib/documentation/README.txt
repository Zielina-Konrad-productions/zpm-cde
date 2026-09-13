IGN-LIB DOCUMENTATION

This directory contains quick reference files for each ign-lib module.

FILES:
ign::print.txt     - printing to terminal
ign::color.txt     - ANSI color and style constants
ign::terminal.txt  - terminal screen and cursor helpers
ign::progressbar.txt - vendored progress bars and spinners
ign::time.txt      - sleep and time helpers
ign::file.txt      - file operations
ign::write.txt     - file writing helpers in ign::file
ign::encrypt.txt   - password-based file encryption in ign::file
ign::config.txt    - key/value config files
ign::catalog.txt   - directory operations
ign::run.txt       - running programs and shell commands
ign::key.txt       - immediate and non-blocking key input
ign::use.txt       - scoped using namespace macros
ign::functions.txt - input, command line arguments, any/all/none
ign::variables.txt - ign::string
ign::random.txt    - pseudo-random numbers, choices, strings, and bytes
ign::status.txt    - shared operation status enum
ign::audio.txt     - simple audio playback through system players


MAIN INCLUDE:
#include <ign.hpp>

This includes the whole library.


BUILD EXAMPLE:
g++ -std=c++17 -I./ign-lib main.cpp -o app


STATUS STYLE:
Most file, directory, config, write, and encryption functions return:
ign::status::ok
ign::status::error
ign::status::not_found
ign::status::permission_denied


PATH STYLE:
Path arguments can start with ~ to mean the current user's home directory.
This works in ign::file, ign::catalog, ign::config, run_to_file(), and
run_shell_to_file(), and ign::audio.

Example:
ign::file::write("~/logs/app.log", "started");

Only ~ and ~/... are expanded. Paths such as ~otheruser/file are left
unchanged.

ign::run() is different:
0  - program success
>0 - program exit code
<0 - process start / library error
