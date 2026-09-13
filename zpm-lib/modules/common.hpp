#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>

//connect files
 #include "printfunctions.hpp"
 #include "color.hpp"

//ign-lib
#include "../../ign-lib/ign.hpp"

namespace zpm::common {

    //version check function
    //usage zpm::common::versioncheck("file localisation, fullpath")
   inline void versioncheck(std::string localisation) {

    std::string BOLD_RED = "\033[1;31m";
    std::string RESET = "\033[0m";

    std::ifstream file(localisation);

    if (!file.is_open()) {

        std::cerr << BOLD_RED << "UNKNOWN" << RESET;
        return;
    } else {

        std::cout << BOLD_RED << file.rdbuf() << RESET;
        return;

    }

    return;
   }

   //autodetect package managers
   class Detection_PM {
    public:
        struct PM {
            bool apt = false;
            bool snap = false;
            bool flatpak_system = false;   
            bool flatpak_user = false;    
            bool flatpak = false;
        };
    
        PM pm;
    
        auto detect_PM() {
            if (ign::run_to_file("/dev/null", "apt --help") == 0) {
                pm.apt = true;
            }
    
            if (ign::run_to_file("/dev/null", "snap --help") == 0) {
                pm.snap = true;
            }
    
            if (ign::run_to_file("/dev/null", "flatpak --help") == 0) {
                pm.flatpak = true;
    
                if (ign::run_to_file("/dev/null", "flatpak remote-list --system | grep -q flathub") == 0) {
                    pm.flatpak_system = true;
                }
                if (ign::run_to_file("/dev/null", "flatpak remote-list --user | grep -q flathub") == 0) {
                    pm.flatpak_user = true;
                }
            }
    
            return pm.apt || pm.snap || pm.flatpak;
        }
    };
    
    inline Detection_PM detection_PM;


    //usage: zpm::common::fix_system("/tmplname.txt" or logpath, false, "update")
    inline void fix_system(const std::string& logpath, bool auto_confirm, const std::string& action_name) {

        bool succes = true;
        char continuation;

        zpm::outl(zpm::color::bold_green, "fix system program:", zpm::color::reset);
        zpm::outl("This program will try to repair broken package managers by running basic repair commands.");
        zpm::outl(' ');

        if (zpm::pythonfunctions::all(!detection_PM.pm.apt, !detection_PM.pm.flatpak, !detection_PM.pm.snap)) {
            zpm::out(zpm::color::bold_red, "ERROR: No package manager found!", zpm::color::reset);
            zpm::outl(' ');
            std::exit(1);
        }

        // APT
        if (detection_PM.pm.apt && succes) {
            zpm::outl(zpm::color::bold_orange, "running repair commands on APT:", zpm::color::reset);
            if (ign::run_to_file(logpath, "dpkg --configure -a", true, true) != 0) {
                succes = false;
            } else {
                zpm::outl("Note: This applies only to APT/dpkg.");
                zpm::outl("If everything is ok, 'dpkg --configure -a' will not show any output.");
                zpm::outl("command done");
                zpm::outl(' ');
            }
        }

        // FLATPAK
        if (detection_PM.pm.flatpak && succes) {
            zpm::outl(zpm::color::bold_orange, "running repair commands on Flatpak:", zpm::color::reset);
            if (ign::run_shell_to_file(logpath, "flatpak repair && flatpak uninstall --unused -y", true, true) != 0) {
                succes = false;
            } else {
                zpm::outl("command done");
                zpm::outl(' ');
            }
        }

        // SNAP
        if (detection_PM.pm.snap && succes) {
            zpm::outl(zpm::color::bold_orange, "running repair commands on Snap:", zpm::color::reset);
            if (ign::run_shell_to_file(logpath, "snap refresh", true, true) != 0) {
                succes = false;
            } else {
                zpm::outl("command done");
                zpm::outl(' ');
            }
        }

        if (!succes) {
            zpm::out(zpm::color::bold_red, "ERROR: A repair command failed!", zpm::color::reset);
            zpm::outl(' ');
            zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
            zpm::outl(logpath);
            std::exit(1);
        }

        zpm::outl(zpm::color::bold_green, "Done!", zpm::color::reset);
        zpm::outl(' ');
        zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
        zpm::outl(logpath);
        zpm::outl(' ');

        zpm::out(zpm::color::bold, "Do you want to continue " + action_name + " ?", zpm::color::reset, " [y/n] ");

        if (auto_confirm) {
            zpm::outl('y');
            continuation = 'y';
        } else {
            std::cin >> continuation;
        }

        if (continuation != 'y' && continuation != 'Y') {
            std::exit(0);
        }
    }
}
