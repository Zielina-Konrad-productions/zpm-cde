//dear reader, this program is made with vibe-coding
//so if it works DONT TOUCH IT

//program structure:
//ask for permision → check dependencies → move directory → creating symlinks → adding exec permisions → endscreen

//PM's - package menagers, e.g. flatpak, snap, apt....
//detect existing PM's class + functions from zpm-lib/modules/common.hpp 

//Days i spend writing this code: 3 days or 72hours

//run localy e.g. ./INSTALL (add exec permisions)

//plik zpm-cde musi byc w /bin, czyli trzeba stowrzyc katalog, usun po stworzeniu calej struktury ignac

//library
#include "../zpm-lib/zpm.hpp"
#include "../ign-lib/ign.hpp"

//normal includes
#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace {

    void helpmessage(){

        zpm::outl(zpm::color::bold_purple, "--help", zpm::color::reset);
        zpm::outl(zpm::color::bold_red, " [options].", zpm::color::reset);
        zpm::outl(' ');
        zpm::outl(zpm::color::bold_orange, "Options:", zpm::color::reset);
        zpm::outl("--help   -h  Show this help message");
        zpm::outl("--yes    -y  Automatic confirmation");
    }

    //variables
    char confirmation_for_install;
    bool instalation_finished_with_errors = false;

    //program arguments
    bool yes = false;
    bool help = false;
}

int main(int argc, char* argv[]) {

    //detect existing PM's
    zpm::common::detection_PM.detect_PM();

    for (int i = 1; i < argc; ++i) {

        //arguments passing
        const std::string arg = argv[i];

        //type arguments passing here
        if (arg == "--help" || arg == "-h") help = true;
        else if (arg == "--yes" || arg == "-y") yes = true;

        //unknown arguments error
        else {
            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, "unknown argument."); 
            return 1;
        } 
    }

    if(help){

        helpmessage();
        return 0;
    }

    //UPDATE LOGIC

    //root
    if (geteuid() != 0) {
        std::cerr << zpm::color::bold_red << "No root privileges!\n" << zpm::color::reset;
        return 1;
    }

    zpm::outl(zpm::color::bold_cyan, "===== zpm-cde local INSTALL =====", zpm::color::reset);
    zpm::out(zpm::color::bold, "[*] Do you want to INSTALL zpm-cde Edition ? [y/n] ", zpm::color::reset);

    if (yes) {
        confirmation_for_install = 'y';
        zpm::outl('y');
    } else {
        std::cin >> confirmation_for_install;
    }

    if (confirmation_for_install != 'y') {
        return 0;
    }

    zpm::outl("[*] Dependencies:");
    zpm::outl("-grep");
    zpm::outl("-curl");
    zpm::outl("-coreutils");
    zpm::outl("-sed");
    zpm::outl(' ');
    zpm::outl("[*] checking dependencies...");

    //make sure that coreutilits are installed
    ign::run_to_file("/dev/null", "apt-get install -y coreutils");

    if (!zpm::common::detection_PM.pm.apt){

            zpm::outl(zpm::color::bold,
                "NOTE, zpm-cde is made for Debian GNU/Linux\n"
                "and apt compatibility for other distros, if you dont use them,\n"
                "zpm will only work with flatpak and snap",
                zpm::color::reset);
            instalation_finished_with_errors = true;
    }

    if (ign::run_to_file("/dev/null", "grep --help") != 0) {

        //check if apt instaled
        if (zpm::common::detection_PM.pm.apt) {

            zpm::outl("[<] installing grep...");

            if (ign::run("apt-get install -y grep") == 0) {
                zpm::outl("> grep installed");

            } else {

                zpm::outl(zpm::color::bold_orange, "[!] Warning grep install failed! — install manually!", zpm::color::reset);
                instalation_finished_with_errors = true;
            }

        } 
    }

    if (ign::run_to_file("/dev/null", "sed --help") != 0) {

        //check if apt instaled
        if (zpm::common::detection_PM.pm.apt) {

            zpm::outl("[<] installing sed...");

            if (ign::run("apt-get install -y sed") == 0) {
                zpm::outl("> sed installed");

            } else {

                zpm::outl(zpm::color::bold_orange, "[!] Warning sed install failed! — install manually!", zpm::color::reset);
                instalation_finished_with_errors = true;
            }

        } 
    }

    if (ign::run_to_file("/dev/null", "curl --help") != 0) {

        //check if apt instaled
        if (zpm::common::detection_PM.pm.apt) {

            zpm::outl("[<] installing curl...");

            if (ign::run("apt-get install -y curl") == 0) {
                zpm::outl("> curl installed");

            } else {

                zpm::outl(zpm::color::bold_orange, "[!] Warning curl install failed! — install manually!", zpm::color::reset);
                instalation_finished_with_errors = true;
            }

        } 
    }
    
    zpm::outl("[*] Starting instalation");

    if (ign::catalog::exists("/opt/zpm-cde") == ign::status::ok) {

        zpm::outl(zpm::color::bold_orange, "[!] existing zpm-cde instalation found!", zpm::color::reset);
        zpm::outl("[<] removing old instalaiton...");

        if (ign::catalog::remove("/opt/zpm-cde") != ign::status::ok) {
            zpm::outl(zpm::color::bold_red, "ERROR: removing old instalation failed!", zpm::color::reset);
            return 1;
        }
    }

    zpm::outl("[*] Moving directory");
    std::filesystem::path working_directory = std::filesystem::current_path();
    std::filesystem::path source = working_directory;

    std::error_code ec_check;
    if (std::filesystem::equivalent(working_directory, "/opt/zpm-cde", ec_check)) {
        zpm::outl(zpm::color::bold_red, "ERROR: installer is already running from /opt/zpm-cde!", zpm::color::reset);
        zpm::outl("Open a new terminal (or 'cd' elsewhere) before running the installer again.");
        return 1;
    }

    if (ign::catalog::move(source.string(), "/opt/zpm-cde") != ign::status::ok) {

    zpm::outl(zpm::color::bold_red, "ERROR: moving directory failed!", zpm::color::reset);
    return 1;
    }

    zpm::outl("[*] Creating symlink");
    std::filesystem::path link = "/usr/bin/zpm-cde";
    std::error_code ec;

    if (std::filesystem::exists(link) || std::filesystem::is_symlink(link)) {

        std::filesystem::remove(link, ec);
        if (ec) {
            zpm::outl(zpm::color::bold_red, "ERROR: removing existing symlink failed! (", ec.message(), ")", zpm::color::reset);
            return 1;
        }
    }

    std::filesystem::create_symlink("/opt/zpm-cde/bin/zpm-cde", link, ec);

    if (ec) {
        zpm::outl(zpm::color::bold_red, "ERROR: creating symlink failed! (", ec.message(), ")", zpm::color::reset);
        return 1;
    }

    zpm::outl("[*] Adding executable permissions");
    std::filesystem::path target = "/opt/zpm-cde/zpm-cde";

    std::filesystem::permissions(
        target,
        std::filesystem::perms::owner_exec |
        std::filesystem::perms::group_exec |
        std::filesystem::perms::others_exec,
        std::filesystem::perm_options::add,
        ec
    );

    if (ec) {
        zpm::outl(zpm::color::bold_red, "ERROR: setting executable permissions failed! (", ec.message(), ")", zpm::color::reset);
        return 1;
    }

    zpm::outl("[*] Finishing instalation");
    
    if (instalation_finished_with_errors) {
        zpm::outl(zpm::color::bold_orange, "[!] Install finished with warnings — check messages above.", zpm::color::reset);
    } else {

        zpm::outl(zpm::color::bold_green, "Install finished", zpm::color::reset);
        zpm::outl("Type zpm-cde --help, for more information.");
    }
    return 0;
}