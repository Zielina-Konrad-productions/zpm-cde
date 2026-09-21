//library
#include "../zpm-lib/zpm.hpp"
#include "../ign-lib/ign.hpp"

//combine files
#include "commands/declaration/update.hpp"
#include "commands/declaration/upgrade.hpp"
#include "commands/declaration/install.hpp"
#include "commands/declaration/remove.hpp"
#include "commands/declaration/uninstall.hpp"

//normal includes
#include <iostream>
#include <filesystem>
#include <string>

//program arguments
bool help = false;
bool version = false;
bool error = false;
bool install = false;
bool update = false;
bool upgrade = false;
bool rm = false;
bool uninstall = false;

//functions
void versionmessage() {

    zpm::outl(zpm::color::bold_purple, "--version", zpm::color::reset);
    zpm::out("ZPM-CDE Edition ver: ");
    zpm::common::versioncheck("/opt/zpm-cde/version.txt");
    zpm::outl('.');
    zpm::outl("Copyright (c) 2026 Ignacyyy");
    zpm::outl("License: MIT");
}

void helpmessage() {

    zpm::outl(zpm::color::bold_purple, "--help", zpm::color::reset);
    zpm::outl(zpm::color::bold_red, "Usage: ", zpm::color::reset, "zpm-cde <command> [options].");
    zpm::outl(' ');
    zpm::outl(zpm::color::bold_orange, "Commands:", zpm::color::reset);
    zpm::out("update, upd       Perform a system update ", '\n', "install, inst     Install packages",
    '\n', "remove, rm        Remove system package", '\n', "upgrade, upgr     Upgrade ZPM-CDE Edition", '\n', "uninstall         Uninstall ZPM-CDE Edition", '\n');
}

bool checkdependencies(const std::string& name) {
    if (ign::run_to_file("/dev/null", name + " --help") != 0) {
        zpm::outl(zpm::color::bold_red, "ERROR: lacking dependency: ", name, zpm::color::reset);
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {

    //checkdependencies
    bool all_ok = true;

    for (const auto& dep : {"grep", "curl", "sed", "tar"}) {
        if (!checkdependencies(dep)) {
            all_ok = false;
        }
    }

    if (!all_ok) {
        zpm::outl(zpm::color::bold_red, "Fatal: missing required system dependencies.", zpm::color::reset);
        return 1;
    }

    //ARGUMENTS LOGIC

    if (argc < 2) {
        zpm::outl(zpm::color::bold_red, "ERROR: ", zpm::color::reset, "no command provided.");
        return 1;
    }

    const std::string command = argv[1];
    if (command == "install" || command == "inst") return run_install(argc - 1, argv + 1);
    if (command == "update"  || command == "upd")  return run_update(argc - 1, argv + 1);
    if (command == "upgrade" || command == "upgr") return run_upgrade(argc - 1, argv + 1);
    if (command == "remove"  || command == "rm")   return run_remove(argc - 1, argv + 1);
    if (command == "uninstall")                    return run_uninstall(argc - 1, argv + 1);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") help = true;
        else if (arg == "--version" || arg == "-v") version = true;
        else {
    
            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " unknown command or option '", arg, "'");
            return 1;
        }
    }

    if (help && version) {
        versionmessage();
        zpm::outl(' ');
        helpmessage();
        return 0;
    }

    if (version) {
        versionmessage(); 
        return 0;
    } 

    if (help) {
        helpmessage(); 
        return 0;
    }

    return 0;
}