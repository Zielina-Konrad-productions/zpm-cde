//dear reader, this program is made with vibe-coding and AI for complicated functions
//so if it works DONT TOUCH IT

//program structure:
//reset → show → check → confirm → update → endscreen

//main function:
//int run_update(int argc, char* argv[]);

//PM's - package menagers, e.g. flatpak, snap, apt....
//detect existing PM's class + functions from zpm-lib/modules/common.hpp 

//fixsystem() wrapper/interpreter for zpm::comon::fix_system()

//Days i spend writing this code: 10 days or 240hours
//most of i spend on fixing bugs.......

//run from zpm-cde.cpp file


//library
#include "../../ign-lib/modules/run.hpp"
#include "../../zpm-lib/zpm.hpp"
#include "../../ign-lib/modules/file.hpp"
#include "../../ign-lib/modules/time.hpp"
#include "../../ign-lib/modules/progressbar.hpp"
#include "../../ign-lib/modules/terminal.hpp"

//combine files
#include "update.hpp"

//normal includes
#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <ranges>
#include <cstdlib>
#include <unistd.h>
#include <algorithm>
#include <thread>

namespace {

    //logpath
    const std::string logpath = "/tmp/zpm-cde-update.txt";

    //checkupdates(); + showpackagestoupdate();
    bool hasupdates;
    bool apthasupdates;
    bool flatpakhasupdates;
    bool snaphasupdates;
    bool checkfailed;

    //confirmationupdate();
    char confirmation_ask;
    bool confirmation = false;

    //update();
    bool updatesucces = false;

    //program arguments
    bool help_update = false;
    bool version_update = false;
    bool automaticconfirmation_update = false;
    bool reboot_update = false;
    bool shutdown_update = false;
    bool fix_system = false;

    void showsystemname() {
        std::ifstream file("/etc/os-release");
            std::string line;
        
            if (!file.is_open()) {
                zpm::outl("System: ", zpm::color::bold_red, "Unknown Linux", zpm::color::reset);
                return;
            }
        
            while (std::getline(file, line)) {
                if (line.rfind("PRETTY_NAME=", 0) == 0) {
                    std::string name = line.substr(12);
        
                    if (!name.empty() && (name.front() == '"' || name.front() == '\'')) name.erase(0, 1);
                    if (!name.empty() && (name.back() == '"' || name.back() == '\'')) name.pop_back();
        
                    zpm::out(zpm::color::orange, "[SYS] ", zpm::color::reset);
                    zpm::outl(name);
                    return;
                }
            }
        }

    void showrepo() {

        zpm::out(zpm::color::orange, "[R] ", zpm::color::reset);
            zpm::outl(zpm::color::bold_green, "Active Repositories:", zpm::color::reset);

            if (!zpm::common::detection_PM.pm.apt && !zpm::common::detection_PM.pm.flatpak && !zpm::common::detection_PM.pm.snap) {

                zpm::outl(' ');
                zpm::outl(zpm::color::bold_red, "- ERROR: Repositories unavailable!", zpm::color::reset);
                zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
                zpm::outl(logpath);
                zpm::outl(' ');
                std::exit(1);
            }

            //APT
            if (zpm::common::detection_PM.pm.apt == true) {

                zpm::outl(' ');
                zpm::out(zpm::color::orange, "[A] ", zpm::color::reset);
                zpm::outl(zpm::color::bold_green, "Apt:", zpm::color::reset);

                bool has_repos = false;

            //OLD FORMAT
            std::ifstream file("/etc/apt/sources.list");
            std::string line;

            if (file.is_open()) {
                while (std::getline(file, line)) {

                line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));

            if (!line.empty() && line[0] != '#') {
                std::cout << zpm::color::orange << "- " << zpm::color::reset << line << std::endl;
                has_repos = true;
            }
        }
    }

        //NEW FORMAT
        if (std::filesystem::exists("/etc/apt/sources.list.d")) {
            for (const auto& entry : std::filesystem::directory_iterator("/etc/apt/sources.list.d")) {

                const std::string ext = entry.path().extension().string();
                if (ext != ".list" && ext != ".sources") continue;

                std::ifstream subfile(entry.path());
                std::string subline;

                while (std::getline(subfile, subline)) {

                subline.erase(subline.begin(), std::find_if(subline.begin(), subline.end(), [](unsigned char ch) {
                    return !std::isspace(ch);
                }));

                if (subline.empty() || subline[0] == '#') continue;

                if (ext == ".sources") {
                    
                    if (subline.rfind("URIs:", 0) == 0) {
                        std::string uri = subline.substr(5);
                        uri.erase(uri.begin(), std::find_if(uri.begin(), uri.end(), [](unsigned char ch) {
                            return !std::isspace(ch);
                        }));
                        std::cout << zpm::color::orange << "- " << zpm::color::reset << uri << std::endl;
                        has_repos = true;
                    }
                    } else {

                    std::cout << zpm::color::orange << "- " << zpm::color::reset << subline << std::endl;
                    has_repos = true;
                    }
                }
            }
        }

            if (!has_repos) {
                zpm::outl(zpm::color::bold_red, "UNKNOWN", zpm::color::reset);
                zpm::outl(' ');
                zpm::outl("NOTE: no APT repositories found in sources.list or sources.list.d.");
                zpm::outl("this may mean an empty/fresh configuration - rest of zpm-cde will still work.");
                zpm::outl(' ');
                ign::sleep_sec(2);
            }

        zpm::outl(' ');
        }

            //FLAPTAK
            if (zpm::common::detection_PM.pm.flatpak == true) {

                zpm::out(zpm::color::orange, "[F] ", zpm::color::reset);
                zpm::outl(zpm::color::bold_green, "Flatpak:", zpm::color::reset);

                ign::run("flatpak remotes");
                zpm::outl(' ');
            }

            //SNAP
            if (zpm::common::detection_PM.pm.snap == true) {

                zpm::out(zpm::color::orange, "[S] ", zpm::color::reset);
                zpm::outl(zpm::color::bold_green, "Snap is available.", zpm::color::reset);
                zpm::outl(' ');

            }
    }

    void helpmessage_update() {
 
        zpm::outl(zpm::color::bold_purple, "--help", zpm::color::reset);
        zpm::outl(zpm::color::bold_red, "Usage: ", zpm::color::reset, "zpm-cde update [options].");
        zpm::outl(' ');
        zpm::outl(zpm::color::bold_orange, "Options:", zpm::color::reset);
        zpm::outl("--yes        -y  Automatic confirmation");
        zpm::outl("--reboot     -r  System reboot after update");
        zpm::outl("--shutdown   -s  System shutdown after update");
        zpm::outl("--help       -h  Show this help message");
        zpm::outl("--version    -v  Show version of zpm-cde");
        zpm::outl("--fix_system -f  Try to fix errors with update");
    }

    void versionmessage_update(){

        zpm::outl(zpm::color::bold_purple, "--version", zpm::color::reset);
        zpm::out("Update component of ZPM-CDE Edition ver: ");
        zpm::common::versioncheck("/opt/zpm-cde/version.txt");
        zpm::outl('.');
        zpm::outl("Copyright (c) 2026 Ignacyyy");
        zpm::outl("License: MIT");
    }

    void reset_update(){

        std::filesystem::remove(logpath);

        hasupdates = false;
        apthasupdates = false;
        flatpakhasupdates = false;
        snaphasupdates = false;
        checkfailed = false;
        confirmation = false;
        updatesucces = false;
    }

    void checkupdates() {

        //info for users
        zpm::outl(zpm::color::bold_orange, "[*] Refreshing package cache...", zpm::color::reset);
        zpm::outl(' ');

        //APT
        if (zpm::common::detection_PM.pm.apt) {
        
            ign::run_shell_to_file(logpath, "apt update 2>/dev/null");
            int res = ign::run_shell_to_file(logpath, "LC_ALL=C apt-get -s -o Debug::NoLocking=true upgrade 2>/dev/null | grep -E '^(Inst|Conf) '", false);
            const auto out = ign::file::read(logpath);
            
            apthasupdates = std::count(out.begin(), out.end(), '\n') > 0;
            checkfailed = checkfailed || res > 1;
        }
    
        //FLATPAK
        if (zpm::common::detection_PM.pm.flatpak) {
            ign::run_shell_to_file(logpath, "flatpak update --appstream 2>/dev/null");
            int res = ign::run_shell_to_file(logpath, "flatpak remote-ls --updates 2>/dev/null | grep -v '^$'", false);
            const auto out = ign::file::read(logpath);
            flatpakhasupdates = std::count(out.begin(), out.end(), '\n') > 0;
            checkfailed = checkfailed || res > 1;
        }
    
        //SNAP
        if (zpm::common::detection_PM.pm.snap) {

            int res = ign::run_shell_to_file(logpath, "LC_ALL=C snap refresh --list 2>/dev/null | grep -v '^Name' | grep -v '^$'", false);
            const auto out = ign::file::read(logpath);
            snaphasupdates = std::count(out.begin(), out.end(), '\n') > 0;
            checkfailed = checkfailed || res != 0;
        }
    
        //no updates + updates detected
        if (!apthasupdates && !flatpakhasupdates && !snaphasupdates) {
            zpm::outl(zpm::color::bold_green, "System is up to date!", zpm::color::reset);
        } else  {
            zpm::outl(zpm::color::bold_cyan, "Updates available!", zpm::color::reset);
            zpm::outl(' ');
            hasupdates = true;
        }
    }

    void showpackagestoupdate() {

        if (hasupdates) {

            //APT
            if (zpm::common::detection_PM.pm.apt && apthasupdates){

                zpm::outl(zpm::color::bold_red, "Packages to update: (APT)", zpm::color::reset);
                ign::run_to_file(logpath, "apt-get -s upgrade", true, true);
                zpm::outl(' ');
            }

            //FLATPAK
            if (zpm::common::detection_PM.pm.flatpak && flatpakhasupdates) {

                zpm::outl(zpm::color::bold_red, "Packages to update: (FLATPAK)", zpm::color::reset);
                ign::run_to_file(logpath, "flatpak remote-ls --updates", true, true);
                zpm::outl(' ');
            }

            //SNAP
            if (zpm::common::detection_PM.pm.snap && snaphasupdates) {
                zpm::outl(zpm::color::bold_red, "Packages to update: (SNAP)", zpm::color::reset);
                ign::run_to_file(logpath, "snap refresh --list", true, true);
                zpm::outl(' ');
            }
        }

    }

    void confirmationupdate() {
        if (!hasupdates) return;
    
        zpm::out(zpm::color::bold_orange, "Do you want to continue ? ", zpm::color::reset, "[y/n] ");
    
        if (automaticconfirmation_update) {
            confirmation_ask = 'y';
            zpm::outl('y');
        } else {
            std::cin >> confirmation_ask;
        }
    
        if (confirmation_ask != 'y') std::exit(1);
    
        zpm::outl(' ');
        confirmation = true;
    }

    void update() {

        if (confirmation && hasupdates) {
            bool status = true;
            bool ran_any = false;

            zpm::outl(zpm::color::bold_cyan, "[Live-LOG]", zpm::color::reset);
            ign::file::print_loopstart(logpath, 4);

            //APT
            if (zpm::common::detection_PM.pm.apt && apthasupdates) {
                int res = ign::run_shell_to_file(
                    logpath,
                    "DEBIAN_FRONTEND=noninteractive apt-get -y full-upgrade"
                );
                ign::run_shell_to_file(
                    logpath,
                    "DEBIAN_FRONTEND=noninteractive apt-get -y autoremove && apt-get clean"
                );
                status = status && (res == 0);
                ran_any = true;
            }

            //FLATPAK
            if (zpm::common::detection_PM.pm.flatpak && flatpakhasupdates) {
                int res = ign::run_to_file(logpath, "flatpak update -y");
                ign::run_to_file(logpath, "flatpak uninstall --unused -y");
                status = status && (res == 0);
                ran_any = true;
            }

            //SNAP
            if (zpm::common::detection_PM.pm.snap && snaphasupdates) {
                int res = ign::run_to_file(logpath, "snap refresh");
                status = status && (res == 0);
                ran_any = true;
            }

            ign::sleep_sec(2);
            ign::file::print_loopend();

            updatesucces = ran_any && status;
        }
    }
    

    void endscreen_update() {

        if (updatesucces == true) {

            zpm::outl(' ');
            zpm::outl(zpm::color::bold_green, "Update succes!", zpm::color::reset);
            zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
            zpm::outl(logpath);
            zpm::outl(' ');
        }  else if ((hasupdates && confirmation) || checkfailed) {

            zpm::outl(' ');
            zpm::outl(zpm::color::bold_red, "Update failed!", zpm::color::reset);
            zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
            zpm::outl(logpath);
            zpm::outl(' ');
        }
    }

    void fixsystem() {
        if (fix_system) {
            zpm::common::fix_system(logpath, automaticconfirmation_update, "update");
        }
    }
} //namespace

//main function
int run_update(int argc, char* argv[]) {

    //detect existing PM's
    zpm::common::detection_PM.detect_PM();

    for (int i = 1; i < argc; ++i) {

        //arguments passing
        const std::string arg = argv[i];

        //type arguments passing here
        if (arg == "--help" || arg == "-h") help_update = true;
        else if (arg == "--version" || arg == "-v") version_update = true;
        else if (arg == "--reboot" || arg == "-r") reboot_update = true;
        else if (arg == "--shutdown" || arg == "-s") shutdown_update = true;
        else if (arg == "--yes" || arg == "-y") automaticconfirmation_update = true;
        else if (arg == "--fix_system" || arg == "-f") fix_system = true;

        //unknown arguments error
        else {
            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, "unknown argument."); 
            return 1;
        } 
    }

    if(zpm::pythonfunctions::all(help_update, version_update) == true) {

        versionmessage_update();
        zpm::outl(' ');
        helpmessage_update();
        return 0;
    }

    if (version_update) {

        versionmessage_update(); 
        return 0;
    } 

    if (help_update) {
        helpmessage_update(); 
        return 0;
    }


    //UPDATE LOGIC

    //root
    if (geteuid() != 0) {
        std::cerr << zpm::color::bold_red << "No root privileges!\n" << zpm::color::reset;
        return 1;
    }

    //program reset, logpath
    reset_update();

    //try to fix broken menagers e.g. apt (OPTIONAL)
    fixsystem();

    //show systemname and repositories
    showsystemname();
    showrepo();

    //check updates
    checkupdates();

    //show packages to update
    showpackagestoupdate();

    //ask for permision to update
    confirmationupdate();

    //perform update
    update();

    //end screen + update succes/failed 
    endscreen_update();


    //AUTOMATIC SHUTDOWN / REBOOT LOGIC

    //shutdown after update
    if (shutdown_update && confirmation) {

        zpm::outl(zpm::color::blue, "Automatic system shutdown in 5 seconds...", zpm::color::reset);
        zpm::outl(' ');
        ign::sleep_sec(5);
        ign::run_shell("shutdown");
        zpm::outl(' ');
        return 0;
    }

    //reboot after update
    if (reboot_update && confirmation) {

        zpm::outl(zpm::color::blue, "Automatic system reboot in 5 seconds...", zpm::color::reset);
        zpm::outl(' ');
        ign::sleep_sec(5);
        ign::run_shell("reboot");
        zpm::outl(' ');
        return 0;
    }

    return 0;
}