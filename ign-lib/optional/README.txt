ign-lib optional installers
===========================

The installer builds small C++ helper programs and installs ign-lib as a
header-only package.

Default install locations:

- Linux: /usr/local/include/ign-lib
- Windows: C:\Program Files\ign-lib

Linux:

    cd ~/ign-lib
    sudo optional/build_INSTALL.sh

Uninstall on Linux:

    cd ~/ign-lib
    sudo optional/build_UNINSTALL.sh

Install into a custom include directory without root:

    optional/build_INSTALL.sh --target "$HOME/.local/include/ign-lib" --yes

Then compile with:

    g++ -std=c++17 main.cpp -I"$HOME/.local/include"

Windows:

    optional\build_INSTALL.bat
    optional\build_UNINSTALL.bat

Run the Windows scripts as Administrator when using the default Program Files
target. For a custom user-writable target, pass:

    optional\build_INSTALL.bat --target "%USERPROFILE%\include\ign-lib" --yes

The build artifacts are written to optional/build/bin.
