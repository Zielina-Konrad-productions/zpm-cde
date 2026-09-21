//dear reader, this program is made with vibe-coding and AI for complicated functions
//so if it works DONT TOUCH IT

//program structure:
//reset → check package names → ask for instalation source →  ask for confirmation → install → endscreen

//main function:
//int run_install(int argc, char* argv[]);

//PM's - package menagers, e.g. flatpak, snap, apt....
//detect existing PM's class + functions from zpm-lib/modules/common.hpp 

//fixsystem() wrapper for zpm::comon::fix_system()

//Days i spend writing this code: 7days or 168hours
//most of i spend on logic of checking and instaling packages
//I HATE WORKING ON VECTORS AND ARRAYS IN GENERAL
//AERGUJIHOQWERFJIOAQRFGJIMEOPQGRFJ3EIOW4JIOYHGTERPQ3RJ5TGIEOP5IQJORGE
//I LOST MY MIND AHAHAAHHAHH

//run from zpm-cde.cpp file


//library
#include "../../ign-lib/modules/run.hpp"
#include "../../zpm-lib/zpm.hpp"
#include "../../ign-lib/modules/file.hpp"
#include "../../ign-lib/modules/time.hpp"

//combine files
#include "install.hpp"

//normal includes
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <limits>

namespace {

    //logpath
    const std::string logpath = "/tmp/zpm-cde-install.txt";

    //install() + finalscreen()
    bool installsucces = false;

    //program arguments
    bool skipconfirmation_install = false;
    bool automaticsource_install = false;
    bool help_install = false;
    bool version_install = false;
    bool fix_system = false;
    std::vector<std::string> packages_to_install;

    //where a package will be installed from, chosen by the user
    enum class InstallSource {
        none,
        apt,
        flatpak,
        snap
    };

    //forces automatic mode to only pick from one source (-oa/-of/-os); none = no forcing
    InstallSource forced_source_install = InstallSource::none;

    //result of checking a single package name against every detected PM
    struct PackageCheckResult {
        std::string name;
        bool found_apt = false;
        bool found_flatpak = false;
        bool found_snap = false;
        InstallSource chosen_source = InstallSource::none;
        bool installed = false;
        std::string flatpak_appid; //exact application ID resolved from 'flatpak search', needed because flatpak install requires it exactly
    
        bool found_any() const {
            return found_apt || found_flatpak || found_snap;
        }
    };

    std::vector<PackageCheckResult> checked_packages;

    void helpmessage_install() {
 
        zpm::outl(zpm::color::bold_purple, "--help", zpm::color::reset);
        zpm::outl(zpm::color::bold_red, "Usage: ", zpm::color::reset, "zpm-cde install <packages> [options].");
        zpm::outl(' ');
        zpm::outl(zpm::color::bold_orange, "Options:", zpm::color::reset);
        zpm::outl("--help          -h   Show this help message");
        zpm::outl("--version       -v   Show version of zpm-cde");
        zpm::outl("--yes           -y   Automatic confirmation");
        zpm::outl("--auto          -a   Automatic instalation");
        zpm::outl("--only-apt      -oa  (forces --auto, hard fail if package isn't in APT)");
        zpm::outl("--only-flatpak  -of  (forces --auto, hard fail if package isn't in Flatpak)");
        zpm::outl("--only-snap     -os  (forces --auto, hard fail if package isn't in Snap)");
        zpm::outl("--fix_system    -f   Try to fix errors with instalation");
    }

    void versionmessage_install(){

        zpm::outl(zpm::color::bold_purple, "--version", zpm::color::reset);
        zpm::out("Update component of ZPM-CDE Edition ver: ");
        zpm::common::versioncheck("/opt/zpm-cde/version.txt");
        zpm::outl('.');
        zpm::outl("Copyright (c) 2026 Ignacyyy");
        zpm::outl("License: MIT");
    }

    void reset_install(){

        std::filesystem::remove(logpath);
        installsucces = false;
    }

    void fixsystem() {
        if (fix_system) {
            zpm::common::fix_system(logpath, skipconfirmation_install, "install");
        }
    }

    //resolves the exact flatpak application ID for a fuzzy-matched name, e.g. "gimp" -> "org.gimp.GIMP"
    //returns empty string if nothing found
    std::string getflatpakappid(const std::string& name){

        std::string cmd = "flatpak search " + name + " --columns=application";
        std::string output = ign::run_to_string(cmd);

        //take first line only
        std::size_t newline_pos = output.find('\n');
        std::string result = (newline_pos == std::string::npos) ? output : output.substr(0, newline_pos);

        return result;
    }

    //helper function for checkpackagenames()
    //checks a package name against every detected PM and returns per-PM results
    PackageCheckResult packageexistsinrepo(const std::string& name) {
        PackageCheckResult result;
        result.name = name;
    
        //APT
        if (zpm::common::detection_PM.pm.apt) {
            auto [out, exit_code] = ign::run_to_string_with_status("apt-cache show " + name);
            result.found_apt = (exit_code == 0) && out.find("Package:") != std::string::npos;
        }
    
        // FLATPAK
        if (zpm::common::detection_PM.pm.flatpak) {
            std::string out = ign::run_to_string("flatpak search " + name);

            std::string out_lower = out;
            std::string name_lower = name;
            std::transform(out_lower.begin(), out_lower.end(), out_lower.begin(), ::tolower);
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

            result.found_flatpak = out_lower.find(name_lower) != std::string::npos;

            if (result.found_flatpak) {
                result.flatpak_appid = getflatpakappid(name);
            }
        }
    
            // SNAP
        if (zpm::common::detection_PM.pm.snap) {
            auto [out, exit_code] = ign::run_to_string_with_status("snap info " + name);
            result.found_snap = (exit_code == 0) && !out.empty();
        }
    
        return result;
    }

    void checkpackagenames(){

        bool any_missing = false;

        for (size_t i = 0; i < packages_to_install.size(); i++) {

            PackageCheckResult result = packageexistsinrepo(packages_to_install[i]);

            if (!result.found_any()) {
                zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " package not found: ", result.name);
                any_missing = true;
                continue;
            }

            checked_packages.push_back(result);
        }

        if (any_missing) {
            std::exit(1);
        }
    }

    //asks the user which source to install a single package from
    InstallSource askforinstalationsource(const PackageCheckResult& pkg){

        zpm::outl(zpm::color::bold, "Package: ",  zpm::color::reset, zpm::color::bold_cyan, pkg.name, zpm::color::reset);

        //APT
        if (pkg.found_apt) {
            zpm::outl(zpm::color::bold, "1. APT: ", zpm::color::reset, zpm::color::bold_green, "exists", zpm::color::reset,
                       " -> ", zpm::color::bold_cyan, pkg.name, zpm::color::reset);
        }
        
        //FLATPAK
        if (pkg.found_flatpak) {
        
            if (pkg.flatpak_appid.empty()) {
                zpm::outl(zpm::color::bold ,"2. Flatpak: ", zpm::color::reset, zpm::color::bold_yellow,
                           "exists (no exact ID found, will fall back to name \"", pkg.name, "\")", zpm::color::reset);
            } else {
                zpm::outl(zpm::color::bold ,"2. Flatpak: ", zpm::color::reset, zpm::color::bold_green, "exists", zpm::color::reset,
                           " -> ", zpm::color::bold_cyan, pkg.flatpak_appid, zpm::color::reset);
            }
        }
        
        //SNAP
        if (pkg.found_snap) {
            zpm::outl(zpm::color::bold, "3. Snap: ", zpm::color::reset, zpm::color::bold_green, "exists", zpm::color::reset,
                       " -> ", zpm::color::bold_cyan, pkg.name, zpm::color::reset);
        }

        while (true) {

            zpm::outl("type instalation source number e.g. 1");
            zpm::out ("choice: ");

            int instalation_source;
            std::cin >> instalation_source;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " not a number, try again.");
                continue;
            }

            zpm::outl(' ');

            if (instalation_source == 1 && pkg.found_apt) {
                return InstallSource::apt;
            }

            if (instalation_source == 2 && pkg.found_flatpak) {
                return InstallSource::flatpak;
            }

            if (instalation_source == 3 && pkg.found_snap) {
                return InstallSource::snap;
            }

            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " that source isn't available for this package, try again.");
        }
    }

    //picks a source automatically for --yes / -y mode, priority: apt > flatpak > snap
    //if forced_source_install is set (-oa/-of/-os), only that source is considered - no fallback
    InstallSource chooseautomatically(const PackageCheckResult& pkg){

        if (forced_source_install == InstallSource::apt) {
            return pkg.found_apt ? InstallSource::apt : InstallSource::none;
        }

        if (forced_source_install == InstallSource::flatpak) {
            return pkg.found_flatpak ? InstallSource::flatpak : InstallSource::none;
        }

        if (forced_source_install == InstallSource::snap) {
            return pkg.found_snap ? InstallSource::snap : InstallSource::none;
        }

        if (pkg.found_apt) {
            return InstallSource::apt;
        }

        if (pkg.found_flatpak) {
            return InstallSource::flatpak;
        }

        if (pkg.found_snap) {
            return InstallSource::snap;
        }

        return InstallSource::none;
    }
    
    //asks the user to confirm before installing, unless skipconfirmation_install is set
    bool askforconfirmation(){

        zpm::outl(zpm::color::bold, "About to install:", zpm::color::reset);

        for (std::size_t i = 0; i < checked_packages.size(); i++) {
            zpm::outl("- ", checked_packages[i].name);
        }

        zpm::outl(' ');
        zpm::out("Proceed? [y/n]: ");

        std::string answer;
        std::cin >> answer;

        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            zpm::outl(' ');
            return false;
        }

        zpm::outl(' ');

        return (answer == "y");
    }
    
    void install() {

        zpm::outl(zpm::color::bold_cyan, "[Live-LOG]", zpm::color::reset);
        ign::file::create(logpath);
        ign::file::print_loopstart(logpath, 4);
        for (std::size_t i = 0; i < checked_packages.size(); i++) {
            const std::string& name = checked_packages[i].name;
            InstallSource source = checked_packages[i].chosen_source;
            if (source == InstallSource::apt) {
                if (ign::run_to_file(logpath, "apt-get install -y " + name) == 0) {
                    checked_packages[i].installed = true;
                }
            } else if (source == InstallSource::flatpak) {
                const std::string& installname = checked_packages[i].flatpak_appid.empty() ? name : checked_packages[i].flatpak_appid;
                if (ign::run_to_file(logpath, "flatpak install -y " + installname) == 0) {
                    checked_packages[i].installed = true;
                }
            } else if (source == InstallSource::snap) {
                if (ign::run_to_file(logpath, "snap install " + name) == 0) {
                    checked_packages[i].installed = true;
                }
            } else if (source == InstallSource::none) {
            }
        }
        ign::file::print_loopend();
    }

    void endscreen_install() {

        zpm::outl(' ');
    
        bool all_success = true;
    
        for (std::size_t i = 0; i < checked_packages.size(); i++) {

            if (checked_packages[i].installed) {
                zpm::outl(zpm::color::bold_green, "OK  ", zpm::color::reset, checked_packages[i].name);
            } else {
                zpm::outl(zpm::color::bold_red, "FAIL", zpm::color::reset, checked_packages[i].name);
                all_success = false;
            }
        }
    
        zpm::outl(' ');
    
        if (all_success) {
            zpm::outl(zpm::color::bold_green, "Install succes!", zpm::color::reset);
        } else {
            zpm::outl(zpm::color::bold_red, "Install finished with errors!", zpm::color::reset);
        }
    
        zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
        zpm::outl(logpath);
        zpm::outl(' ');
    }
} //namespace

//main function
int run_install(int argc, char* argv[]) {
    
    //detect existing PM's
    zpm::common::detection_PM.detect_PM();

    for (int i = 1; i < argc; ++i) {

        //arguments passing
        const std::string arg = argv[i];

        //type arguments passing here
        if (arg == "--help" || arg == "-h") help_install = true;
        else if (arg == "--version" || arg == "-v") version_install = true;
        else if (arg == "--yes" || arg == "-y") skipconfirmation_install = true;
        else if (arg == "--auto" || arg == "-a") automaticsource_install = true;
        else if (arg == "--only-apt" || arg == "-oa") { forced_source_install = InstallSource::apt; automaticsource_install = true; }
        else if (arg == "--only-flatpak" || arg == "-of") { forced_source_install = InstallSource::flatpak; automaticsource_install = true; }
        else if (arg == "--only-snap" || arg == "-os") { forced_source_install = InstallSource::snap; automaticsource_install = true; }
        else if (arg == "--fix_system" || arg == "-f") fix_system = true;

        //unknown argument
        else if (!arg.empty() && arg[0] == '-') {
            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, "unknown argument."); 
            return 1;
        }

        //rest = package name
        else {
            packages_to_install.push_back(arg);
        }
    }

    if(zpm::pythonfunctions::all(help_install, version_install) == true) {

        versionmessage_install();
        zpm::outl(' ');
        helpmessage_install();
        return 0;
    }

    if (version_install) {

        versionmessage_install(); 
        return 0;
    } 

    if (help_install) {
        helpmessage_install(); 
        return 0;
    }

    //INSTALATION LOGIC

    //if no packages
    if (packages_to_install.empty()) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " no packages specified.");
        return 1;
    }


    //root
    if (geteuid() != 0) {
        std::cerr << zpm::color::bold_red << "no root privileges!\n" << zpm::color::reset;
        return 1;
    }

    //no supported package manager found on the system at all
    if (!zpm::common::detection_PM.pm.apt && !zpm::common::detection_PM.pm.flatpak && !zpm::common::detection_PM.pm.snap) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " no supported package manager (apt/flatpak/snap) detected on this system.");
        return 1;
    }

    //program reset, logpath
    reset_install();

    //try to fix broken menagers e.g. apt (OPTIONAL)
    fixsystem();

    //forced source mode: fail early if the requested PM isn't even installed on this system
    if (forced_source_install == InstallSource::apt && !zpm::common::detection_PM.pm.apt) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " apt is not installed on this system.");
        return 1;
    }
    if (forced_source_install == InstallSource::flatpak && !zpm::common::detection_PM.pm.flatpak) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " flatpak is not installed on this system.");
        return 1;
    }
    if (forced_source_install == InstallSource::snap && !zpm::common::detection_PM.pm.snap) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " snap is not installed on this system.");
        return 1;
    }

    //check if packages exists in repo
    checkpackagenames();

    //ask user which source to use for each package (skip prompt in automatic mode)
    for (size_t i = 0; i < checked_packages.size(); i++) {
        checked_packages[i].chosen_source = automaticsource_install
            ? chooseautomatically(checked_packages[i])
            : askforinstalationsource(checked_packages[i]);
    }

    //forced source mode (-oa/-of/-os): hard fail if any package isn't available in that source
    if (forced_source_install != InstallSource::none) {

        bool any_unavailable = false;

        for (size_t i = 0; i < checked_packages.size(); i++) {
            if (checked_packages[i].chosen_source == InstallSource::none) {
                zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " package not available in forced source: ", checked_packages[i].name);
                any_unavailable = true;
            }
        }

        if (any_unavailable) {
            return 1;
        }
    }

    //ask for confirmation before installing (skip in -y mode)
    if (!skipconfirmation_install && !askforconfirmation()) {
        zpm::outl(zpm::color::bold_red, "Aborted.", zpm::color::reset);
        return 1;
    }

    //perform instalation of packages
    install();
    
    //end screen + install succes/failed
    endscreen_install();

    return 0;
}