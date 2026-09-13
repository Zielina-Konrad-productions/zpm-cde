//dear reader, this program is made with vibe-coding and AI for complicated functions
//so if it works DONT TOUCH IT

//program structure:
//reset → check installed packages → ask for removal source → ask for confirmation → remove → endscreen

//main function:
//int run_remove(int argc, char* argv[]);

//PM's - package menagers, e.g. flatpak, snap, apt....
//detect existing PM's class + functions from zpm-lib/modules/common.hpp

//fixsystem() wrapper for zpm::comon::fix_system()

//Days i spend writing this code: none - 15min
//remove function is just is inverse process of instalation
//so i pasted code to claude and did in in 15min
//90% of funcitions are the same, or with minor changes

//run from zpm-cde.cpp file


//library
#include "../../ign-lib/modules/run.hpp"
#include "../../zpm-lib/zpm.hpp"
#include "../../ign-lib/modules/file.hpp"
#include "../../ign-lib/modules/time.hpp"

//combine files
#include "remove.hpp"

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
    const std::string logpath = "/tmp/zpm-cde-remove.txt";

    //remove() + finalscreen()
    bool removesucces = false;

    //program arguments
    bool skipconfirmation_remove = false;
    bool automaticsource_remove = false;
    bool help_remove = false;
    bool version_remove = false;
    bool fix_system = false;
    std::vector<std::string> packages_to_remove;

    //where a package will be removed from, chosen by the user
    enum class RemoveSource {
        none,
        apt,
        flatpak,
        snap
    };

    //forces automatic mode to only pick from one source (-oa/-of/-os); none = no forcing
    RemoveSource forced_source_remove = RemoveSource::none;

    //result of checking a single package name against every detected PM
    struct PackageCheckResult {
        std::string name;
        bool found_apt = false;
        bool found_flatpak = false;
        bool found_snap = false;
        RemoveSource chosen_source = RemoveSource::none;
        bool removed = false;
        std::string flatpak_appid; //exact application ID resolved from 'flatpak list', needed because flatpak uninstall requires it exactly
    
        bool found_any() const {
            return found_apt || found_flatpak || found_snap;
        }
    };

    std::vector<PackageCheckResult> checked_packages;

    void helpmessage_remove() {
 
        zpm::outl(zpm::color::bold_purple, "--help", zpm::color::reset);
        zpm::outl(zpm::color::bold_red, "Usage: ", zpm::color::reset, "zpm-cde remove <packages> [options].");
        zpm::outl(' ');
        zpm::outl(zpm::color::bold_orange, "Options:", zpm::color::reset);
        zpm::outl("--help          -h   Show this help message");
        zpm::outl("--version       -v   Show version of zpm-cde");
        zpm::outl("--yes           -y   Automatic confirmation");
        zpm::outl("--auto          -a   Automatic removal");
        zpm::outl("--only-apt      -oa  (forces --auto, hard fail if package isn't installed via APT)");
        zpm::outl("--only-flatpak  -of  (forces --auto, hard fail if package isn't installed via Flatpak)");
        zpm::outl("--only-snap     -os  (forces --auto, hard fail if package isn't installed via Snap)");
        zpm::outl("--fix_system    -f   Try to fix errors with removal");
    }

    void versionmessage_remove(){

        zpm::outl(zpm::color::bold_purple, "--version", zpm::color::reset);
        zpm::out("Update component of ZPM-CDE Edition ver: ");
        zpm::common::versioncheck("version.txt");
        zpm::outl('.');
        zpm::outl("Copyright (c) 2026 Ignacyyy");
        zpm::outl("License: MIT");
    }

    void reset_remove(){

        std::filesystem::remove(logpath);
        removesucces = false;
    }

    void fixsystem() {
        if (fix_system) {
            zpm::common::fix_system(logpath, skipconfirmation_remove, "remove");
        }
    }

    //resolves the exact flatpak application ID for an installed, fuzzy-matched name, e.g. "gimp" -> "org.gimp.GIMP"
    //returns empty string if nothing found
    std::string getflatpakappid(const std::string& name){

        std::string cmd = "flatpak list --app --columns=application";
        std::string output = ign::run_to_string(cmd);

        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        std::size_t pos = 0;
        while (pos < output.size()) {

            std::size_t newline_pos = output.find('\n', pos);
            std::string line = (newline_pos == std::string::npos) ? output.substr(pos) : output.substr(pos, newline_pos - pos);

            std::string line_lower = line;
            std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);

            if (line_lower.find(name_lower) != std::string::npos) {
                return line;
            }

            if (newline_pos == std::string::npos) break;
            pos = newline_pos + 1;
        }

        return "";
    }

    //helper function for checkpackagenames()
    //checks a package name against every detected PM's INSTALLED packages and returns per-PM results
    PackageCheckResult packageinstalledinpm(const std::string& name) {
        PackageCheckResult result;
        result.name = name;
    
        //APT
        if (zpm::common::detection_PM.pm.apt) {
            std::string out = ign::run_to_string("apt-cache policy " + name);
            result.found_apt = out.find("Installed: (none)") == std::string::npos
                             && out.find("Installed:") != std::string::npos;
        }
    
        // FLATPAK
        if (zpm::common::detection_PM.pm.flatpak) {
            std::string out = ign::run_to_string("flatpak list --app --columns=application,name");

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
                auto [out, exit_code] = ign::run_to_string_with_status("snap list " + name);
                result.found_snap = (exit_code == 0) && !out.empty();
            }
    
        return result;
    }

    void checkpackagenames(){

        bool any_missing = false;

        for (size_t i = 0; i < packages_to_remove.size(); i++) {

            PackageCheckResult result = packageinstalledinpm(packages_to_remove[i]);

            if (!result.found_any()) {
                zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " package not installed: ", result.name);
                any_missing = true;
                continue;
            }

            checked_packages.push_back(result);
        }

        if (any_missing) {
            std::exit(1);
        }
    }

    //asks the user which source to remove a single package from
    RemoveSource askforremovalsource(const PackageCheckResult& pkg){

        zpm::outl(zpm::color::bold, "Package: ",  zpm::color::reset, zpm::color::bold_cyan, pkg.name, zpm::color::reset);

        //APT
        if (pkg.found_apt) {
            zpm::outl(zpm::color::bold, "1. APT: ", zpm::color::reset, zpm::color::bold_green, "installed", zpm::color::reset,
                       " -> ", zpm::color::bold_cyan, pkg.name, zpm::color::reset);
        }
        
        //FLATPAK
        if (pkg.found_flatpak) {
        
            if (pkg.flatpak_appid.empty()) {
                zpm::outl(zpm::color::bold ,"2. Flatpak: ", zpm::color::reset, zpm::color::bold_yellow,
                           "installed (no exact ID found, will fall back to name \"", pkg.name, "\")", zpm::color::reset);
            } else {
                zpm::outl(zpm::color::bold ,"2. Flatpak: ", zpm::color::reset, zpm::color::bold_green, "installed", zpm::color::reset,
                           " -> ", zpm::color::bold_cyan, pkg.flatpak_appid, zpm::color::reset);
            }
        }
        
        //SNAP
        if (pkg.found_snap) {
            zpm::outl(zpm::color::bold, "3. Snap: ", zpm::color::reset, zpm::color::bold_green, "installed", zpm::color::reset,
                       " -> ", zpm::color::bold_cyan, pkg.name, zpm::color::reset);
        }

        while (true) {

            zpm::outl("type removal source number e.g. 1");
            zpm::out ("choice: ");

            int removal_source;
            std::cin >> removal_source;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " not a number, try again.");
                continue;
            }

            zpm::outl(' ');

            if (removal_source == 1 && pkg.found_apt) {
                return RemoveSource::apt;
            }

            if (removal_source == 2 && pkg.found_flatpak) {
                return RemoveSource::flatpak;
            }

            if (removal_source == 3 && pkg.found_snap) {
                return RemoveSource::snap;
            }

            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " that source isn't available for this package, try again.");
        }
    }

    //picks a source automatically for --yes / -y mode, priority: apt > flatpak > snap
    //if forced_source_remove is set (-oa/-of/-os), only that source is considered - no fallback
    RemoveSource chooseautomatically(const PackageCheckResult& pkg){

        if (forced_source_remove == RemoveSource::apt) {
            return pkg.found_apt ? RemoveSource::apt : RemoveSource::none;
        }

        if (forced_source_remove == RemoveSource::flatpak) {
            return pkg.found_flatpak ? RemoveSource::flatpak : RemoveSource::none;
        }

        if (forced_source_remove == RemoveSource::snap) {
            return pkg.found_snap ? RemoveSource::snap : RemoveSource::none;
        }

        if (pkg.found_apt) {
            return RemoveSource::apt;
        }

        if (pkg.found_flatpak) {
            return RemoveSource::flatpak;
        }

        if (pkg.found_snap) {
            return RemoveSource::snap;
        }

        return RemoveSource::none;
    }
    
    //asks the user to confirm before removing, unless skipconfirmation_remove is set
    bool askforconfirmation(){

        zpm::outl(' ');
        zpm::outl(zpm::color::bold, "About to remove:", zpm::color::reset);

        for (std::size_t i = 0; i < checked_packages.size(); i++) {
            zpm::outl("- ", checked_packages[i].name);
        }

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

    void removepackages() {

        zpm::outl(zpm::color::bold_cyan, "[Live-LOG]", zpm::color::reset);
        ign::file::create(logpath);
        ign::file::print_loopstart(logpath, 4);
        for (std::size_t i = 0; i < checked_packages.size(); i++) {
            const std::string& name = checked_packages[i].name;
            RemoveSource source = checked_packages[i].chosen_source;
            if (source == RemoveSource::apt) {
                if (ign::run_to_file(logpath, "apt remove -y " + name) == 0) {
                    checked_packages[i].removed = true;
                }
            } else if (source == RemoveSource::flatpak) {
                const std::string& removename = checked_packages[i].flatpak_appid.empty() ? name : checked_packages[i].flatpak_appid;
                if (ign::run_to_file(logpath, "flatpak uninstall -y " + removename) == 0) {
                    checked_packages[i].removed = true;
                }
            } else if (source == RemoveSource::snap) {
                if (ign::run_to_file(logpath, "snap remove " + name) == 0) {
                    checked_packages[i].removed = true;
                }
            } else if (source == RemoveSource::none) {
            }
        }
        ign::file::print_loopend();
    }

    void endscreen_remove() {

        zpm::outl(' ');
    
        bool all_success = true;
    
        for (std::size_t i = 0; i < checked_packages.size(); i++) {

            if (checked_packages[i].removed) {
                zpm::outl(zpm::color::bold_green, "OK  ", zpm::color::reset, checked_packages[i].name);
            } else {
                zpm::outl(zpm::color::bold_red, "FAIL", zpm::color::reset, checked_packages[i].name);
                all_success = false;
            }
        }
    
        zpm::outl(' ');
    
        if (all_success) {
            zpm::outl(zpm::color::bold_green, "Remove succes!", zpm::color::reset);
        } else {
            zpm::outl(zpm::color::bold_red, "Remove finished with errors!", zpm::color::reset);
        }
    
        zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
        zpm::outl(logpath);
        zpm::outl(' ');
    }
} //namespace

//main function
int run_remove(int argc, char* argv[]) {
    
    //detect existing PM's
    zpm::common::detection_PM.detect_PM();

    for (int i = 1; i < argc; ++i) {

        //arguments passing
        const std::string arg = argv[i];

        //type arguments passing here
        if (arg == "--help" || arg == "-h") help_remove = true;
        else if (arg == "--version" || arg == "-v") version_remove = true;
        else if (arg == "--yes" || arg == "-y") skipconfirmation_remove = true;
        else if (arg == "--auto" || arg == "-a") automaticsource_remove = true;
        else if (arg == "--only-apt" || arg == "-oa") { forced_source_remove = RemoveSource::apt; automaticsource_remove = true; }
        else if (arg == "--only-flatpak" || arg == "-of") { forced_source_remove = RemoveSource::flatpak; automaticsource_remove = true; }
        else if (arg == "--only-snap" || arg == "-os") { forced_source_remove = RemoveSource::snap; automaticsource_remove = true; }
        else if (arg == "--fix_system" || arg == "-f") fix_system = true;

        //unknown argument
        else if (!arg.empty() && arg[0] == '-') {
            zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, "unknown argument."); 
            return 1;
        }

        //rest = package name
        else {
            packages_to_remove.push_back(arg);
        }
    }

    if(zpm::pythonfunctions::all(help_remove, version_remove) == true) {

        versionmessage_remove();
        zpm::outl(' ');
        helpmessage_remove();
        return 0;
    }

    if (version_remove) {

        versionmessage_remove(); 
        return 0;
    } 

    if (help_remove) {
        helpmessage_remove(); 
        return 0;
    }

    //REMOVAL LOGIC

    //if no packages
    if (packages_to_remove.empty()) {
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
    reset_remove();

    //try to fix broken menagers e.g. apt (OPTIONAL)
    fixsystem();

    //forced source mode: fail early if the requested PM isn't even installed on this system
    if (forced_source_remove == RemoveSource::apt && !zpm::common::detection_PM.pm.apt) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " apt is not installed on this system.");
        return 1;
    }
    if (forced_source_remove == RemoveSource::flatpak && !zpm::common::detection_PM.pm.flatpak) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " flatpak is not installed on this system.");
        return 1;
    }
    if (forced_source_remove == RemoveSource::snap && !zpm::common::detection_PM.pm.snap) {
        zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " snap is not installed on this system.");
        return 1;
    }

    //check if packages are installed via any PM
    checkpackagenames();

    //ask user which source to use for each package (skip prompt in automatic mode)
    for (size_t i = 0; i < checked_packages.size(); i++) {
        checked_packages[i].chosen_source = automaticsource_remove
            ? chooseautomatically(checked_packages[i])
            : askforremovalsource(checked_packages[i]);
    }

    //forced source mode (-oa/-of/-os): hard fail if any package isn't available in that source
    if (forced_source_remove != RemoveSource::none) {

        bool any_unavailable = false;

        for (size_t i = 0; i < checked_packages.size(); i++) {
            if (checked_packages[i].chosen_source == RemoveSource::none) {
                zpm::outl(zpm::color::bold_red, "ERROR:", zpm::color::reset, " package not available in forced source: ", checked_packages[i].name);
                any_unavailable = true;
            }
        }

        if (any_unavailable) {
            return 1;
        }
    }

    //ask for confirmation before removing (skip in -y mode)
    if (!skipconfirmation_remove && !askforconfirmation()) {
        zpm::outl(zpm::color::bold_red, "Aborted.", zpm::color::reset);
        return 1;
    }

    //perform removal of packages
    removepackages();
    
    //end screen + remove succes/failed
    endscreen_remove();

    return 0;
}