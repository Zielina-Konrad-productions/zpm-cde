//dear reader, this program is made with vibe-coding
//so if it works DONT TOUCH IT

//program structure:
//ask for permision → remove zpm-cde directory → remove symlink → endscreen

//PM's - package menagers, e.g. flatpak, snap, apt....
//detect existing PM's class + functions from zpm-lib/modules/common.hpp 

//Days i spend writing this code: 2hours

//run from zpm-cde.cpp file


//library
#include "../zpm-lib/zpm.hpp"
#include "../ign-lib/ign.hpp"

//combine files
#include "uninstall.hpp"

//normal inclides
#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <unistd.h>

namespace {

    //variables
    char confirmation_for_uninstall;
    bool uninstall_finished_with_errors = false;

    //program arguments
    bool yes = false;
    bool help = false;
    bool version = false;

    void helpmessage(){

        zpm::outl(zpm::color::bold_purple, "--help", zpm::color::reset);
        zpm::outl(zpm::color::bold_red, "Usage: ", zpm::color::reset, "zpm-cde uninstall [options].");
        zpm::outl(' ');
        zpm::outl(zpm::color::bold_orange, "Options:", zpm::color::reset);
        zpm::outl("--help       -h  Show this help message");
        zpm::outl("--version    -v  Show version of zpm-cde");
        zpm::outl("--yes        -y  Automatic confirmation");
    }

    void versionmessage(){

        zpm::outl(zpm::color::bold_purple, "--version", zpm::color::reset);
        zpm::out("Update component of ZPM-CDE Edition ver: ");
        zpm::common::versioncheck("../version.txt");
        zpm::outl('.');
        zpm::outl("Copyright (c) 2026 Ignacyyy");
        zpm::outl("License: MIT");
    }
}

//main function
int run_uninstall(int argc, char* argv[]) {

    //detect existing PM's
    zpm::common::detection_PM.detect_PM();

    for (int i = 1; i < argc; ++i) {

        //arguments passing
        const std::string arg = argv[i];

        //type arguments passing here
        if (arg == "--help" || arg == "-h") help = true;
        else if (arg == "--yes" || arg == "-y") yes = true;
        else if (arg == "--version" || arg == "-v") version = true;

        //unknown arguments error
        else {
            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, "unknown argument."); 
            return 1;
        } 
    }

    if (zpm::pythonfunctions::all(version, help)) {

        helpmessage();
        zpm::outl(' ');
        versionmessage();
        return 0;
    }

    if (help) {

        helpmessage();
        return 0;
    }

    if (version) {

        versionmessage();
        return 0;
    }

    //UNINSTALL LOGIC

    //root
    if (geteuid() != 0) {
        std::cerr << zpm::color::bold_red << "No root privileges!\n" << zpm::color::reset;
        return 1;
    }

    zpm::outl(zpm::color::bold_cyan, "===== zpm-cde UNINSTALL =====", zpm::color::reset);
    zpm::out(zpm::color::bold, "[*] Do you want to UNINSTALL zpm-cde Edition ? [y/n] ", zpm::color::reset);

    if (yes) {
        confirmation_for_uninstall = 'y';
        zpm::outl('y');
    } else {
        std::cin >> confirmation_for_uninstall;
    }

    if (confirmation_for_uninstall != 'y') {
        return 0;
    }

    zpm::outl("[*] Starting...");

    if (ign::file::exists("/usr/bin/zpm-cde") == ign::status::ok) {

        zpm::outl("[*] Removing symlink");
        if (ign::file::remove("/usr/bin/zpm-cde") != ign::status::ok) {
            zpm::outl(zpm::color::bold_orange, "[!]: removing symlink failed - Remove manually!", zpm::color::reset);
            zpm::outl(zpm::color::bold_orange,"location: /usr/bin/zpm-cde", zpm::color::reset);
            uninstall_finished_with_errors = true;
        }
    }

    if (ign::catalog::exists("/opt/zpm-cde") == ign::status::ok) {

        zpm::outl("[*] removing zpm-cde");
        if (ign::catalog::remove("/opt/zpm-cde") != ign::status::ok) {
            zpm::outl(zpm::color::bold_red, "ERROR: removing instalation failed!", zpm::color::reset);
            return 1;
        }
    }

    zpm::outl("[*] Finishing...");
    if (uninstall_finished_with_errors) {
        zpm::outl(zpm::color::bold_orange, "[!] Uninstall finished with warnings — check messages above.", zpm::color::reset);
    } else {

        zpm::outl(zpm::color::bold_green, "Uninstall finished", zpm::color::reset);
        zpm::outl("Sorry to see you go :(");
    }

    return 0;
}