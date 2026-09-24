//library
#include "../../zpm-lib/zpm.hpp"
#include "../../ign-lib/ign.hpp"

//combine files
#include "upgrade.hpp"

//normal includes
#include <iostream>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <unistd.h>

namespace {

    //logpath
    const std::string logpath = "/tmp/zpm-cde-upgrade.txt";

    //installed version.txt location
    constexpr const char* VERSION_FILE_PATH = "/opt/zpm-cde/version.txt";

    //variables
    bool upgradeavialable = false;
    bool upgradesucces = true;

    //program arguments
    bool help = false;
    bool version = false;
    bool yes = false;

    void helpmessage() {

        zpm::outl(zpm::color::bold_purple, "--help", zpm::color::reset);
        zpm::outl(zpm::color::bold_red, "Usage: ", zpm::color::reset, "zpm-cde upgrade [options].");
        zpm::outl(' ');
        zpm::outl(zpm::color::bold_orange, "Options:", zpm::color::reset);
        zpm::outl("--yes        -y  Automatic confirmation");
        zpm::outl("--help       -h  Show this help message");
        zpm::outl("--version    -v  Show version of zpm-cde");
    }

    void versionmessage() {
        zpm::outl(zpm::color::bold_purple, "--version", zpm::color::reset);
        zpm::out("Upgrade component of ZPM-CDE Edition ver: ");
        zpm::common::versioncheck("/opt/zpm-cde/version.txt");
        zpm::outl('.');
        zpm::outl("Copyright (c) 2026 Ignacyyy");
        zpm::outl("License: MIT");
    }

    // returns: -1 if a < b, 0 if a == b, 1 if a > b
    int compareversions(std::string a, std::string b) {
        auto strip_v = [](std::string& s) {
            if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
            s.erase(0, 1);
            }
        };
        strip_v(a);
        strip_v(b);

        auto parse = [](const std::string& v) {
        std::vector<int> parts;
        std::stringstream ss(v);
        std::string segment;
            while (std::getline(ss, segment, '.')) {
            parts.push_back(std::stoi(segment));
            }
            return parts;
        };

        std::vector<int> va = parse(a);
        std::vector<int> vb = parse(b);

        size_t len = std::max(va.size(), vb.size());
        va.resize(len, 0);
        vb.resize(len, 0);

        for (size_t i = 0; i < len; ++i) {
            if (va[i] < vb[i]) return -1;
            if (va[i] > vb[i]) return 1;
        }
        return 0;
    }

    std::string printlatestversion() {
        std::string command = "curl -s https://api.github.com/repos/Zielina-Konrad-productions/zpm-cde/releases/latest | sed -n 's/.*\"tag_name\":[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p'";
        std::string latestversion = ign::run_shell_to_string(command);
    
        while (!latestversion.empty() &&
               (latestversion.back() == '\n' || latestversion.back() == '\r' || latestversion.back() == ' ')) {
            latestversion.pop_back();
        }
    
        if (latestversion.empty()) {
            zpm::out(zpm::color::bold_red, "UNKNOWN", zpm::color::reset);
        } else {
            zpm::out(latestversion);
        }
    
        return latestversion;
    }

    void upgrade(){

        char confirmation;

        if (upgradeavialable) {

            zpm::outl(zpm::color::bold_cyan, "Upgrade available!", zpm::color::reset);
            zpm::outl(' ');
            zpm::out(zpm::color::bold_orange, "Do you want to continue ?", zpm::color::reset);
            zpm::out(" [y/n] ");
            if (yes) {
                confirmation = 'y';
                zpm::outl(' ');
            } else {
                std::cin >> confirmation;
                zpm::outl(' ');
            }

            if (confirmation != 'y') {
                std::exit(0);
            }

            //create and clean catalog
            zpm::outl("[*] cleaning /tmp/zpm-cde");

            if (ign::catalog::create("/tmp/zpm-cde") != ign::status::ok) {
                zpm::outl(' ');
                zpm::outl(zpm::color::bold_red, "ERROR: could not create /tmp/zpm-cde", zpm::color::reset);
                upgradesucces = false;
                return;
            }

            if (ign::catalog::clear("/tmp/zpm-cde") != ign::status::ok) {
                zpm::outl(' ');
                zpm::outl(zpm::color::bold_red, "ERROR: cleaning failed", zpm::color::reset);
                upgradesucces = false;
                return;
            }

            //download latest version
            zpm::outl("[*] downloading latest zpm-cde version");
            std::string command =
            "set -eo pipefail; "
            "url=$(curl -fsSL https://api.github.com/repos/Zielina-Konrad-productions/zpm-cde/releases/latest "
            "| sed -n 's/.*\"browser_download_url\": *\"\\([^\"]*\\.tar\\.gz\\)\".*/\\1/p' | head -n1); "
            "if [ -z \"$url\" ]; then echo 'could not resolve asset url' >&2; exit 1; fi; "
            "curl -fsSL \"$url\" | tar -xz -C /tmp/zpm-cde";

            if (ign::run_shell_to_file(logpath, command) != 0) {
                zpm::outl(' ');
                zpm::outl(zpm::color::bold_red, "ERROR: downloading new version failed!", zpm::color::reset);
                upgradesucces = false;
                return;
            }

            zpm::outl("[*] installing to /opt/zpm-cde");
            if (ign::catalog::exists("/opt/zpm-cde") == ign::status::ok) {
                if (ign::catalog::remove("/opt/zpm-cde") != ign::status::ok) {
                    zpm::outl(' ');
                    zpm::outl(zpm::color::bold_red, "ERROR: could not remove old /opt/zpm-cde", zpm::color::reset);
                    upgradesucces = false;
                    return;
                    }
            }

            if (ign::catalog::move("/tmp/zpm-cde", "/opt/zpm-cde") != ign::status::ok) {
                zpm::outl(' ');
                zpm::outl(zpm::color::bold_red, "ERROR: installing new version failed", zpm::color::reset);
                upgradesucces = false;
                return;
            }

            zpm::outl("[*] Updating symlink");
            std::filesystem::path link = "/usr/bin/zpm-cde";
            std::error_code ec;

            if (std::filesystem::exists(link) || std::filesystem::is_symlink(link)) {
                std::filesystem::remove(link, ec);
                if (ec) {
                    zpm::outl(' ');
                    zpm::outl(zpm::color::bold_red, "ERROR: removing existing symlink failed! (", ec.message(), ")", zpm::color::reset);
                    upgradesucces = false;
                    return;
                }
            }

            std::filesystem::create_symlink("/opt/zpm-cde/bin/zpm-cde", link, ec);
                if (ec) {
                    zpm::outl(' ');
                    zpm::outl(zpm::color::bold_red, "ERROR: creating symlink failed! (", ec.message(), ")", zpm::color::reset);
                    upgradesucces = false;
                    return;
                }

            zpm::outl("[*] Adding executable permissions");
            std::filesystem::path target = "/opt/zpm-cde/bin/zpm-cde";

            std::filesystem::permissions(
                target,
                std::filesystem::perms::owner_exec |
                std::filesystem::perms::group_exec |
                std::filesystem::perms::others_exec,
                std::filesystem::perm_options::add,
                ec
            );

            if (ec) {
                zpm::outl(' ');
                zpm::outl(zpm::color::bold_red, "ERROR: setting executable permissions failed! (", ec.message(), ")", zpm::color::reset);
                upgradesucces = false;
                return;
            }
        }
    }

    void endscreen(){

        if (upgradeavialable && upgradesucces ) {

            zpm::outl(' ');
            zpm::outl(zpm::color::bold_green, "Upgrade succes!", zpm::color::reset);
            zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
            zpm::outl(logpath);
            zpm::outl(' ');
        } else {

            zpm::outl(' ');
            zpm::outl(zpm::color::bold_red, "Upgrade failed!", zpm::color::reset);
            zpm::outl(zpm::color::bold, "LOGFILE:", zpm::color::reset);
            zpm::outl(logpath);
            zpm::outl(' ');
        }
    }
}

int run_upgrade(int argc, char* argv[]) {
    
    //detect existing PM's
    zpm::common::detection_PM.detect_PM();

    for (int i = 1; i < argc; ++i) {

        //arguments passing
        const std::string arg = argv[i];

        //type arguments passing here
        if (arg == "--help" || arg == "-h") help = true;
        else if (arg == "--version" || arg == "-v") version = true;
        else if (arg == "--yes" || arg == "-y") yes = true;

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

    //UPGRADE LOGIC

    //root
    if (geteuid() != 0) {
        std::cerr << zpm::color::bold_red << "no root privileges!\n" << zpm::color::reset;
        return 1;
    }

    zpm::outl(zpm::color::bold_red, "zpm-cde upgrade program", zpm::color::reset);
    zpm::outl(' ');
    zpm::outl(zpm::color::bold, "zpm-cde versions:", zpm::color::reset);
    zpm::outl(zpm::color::cyan, "----------------------------------------------------", zpm::color::reset);
    zpm::out("zpm-cde installed version: ");  zpm::common::versioncheck(VERSION_FILE_PATH); zpm::outl('.');
    zpm::out("zpm-cde latest version: "); std::string latest = printlatestversion(); zpm::outl('.');
    zpm::outl(zpm::color::cyan, "----------------------------------------------------", zpm::color::reset);

    std::string local = zpm::common::versioncheck_string(VERSION_FILE_PATH);

    if (compareversions(latest, local) > 0) {
        upgradeavialable = true;
        zpm::outl(' ');
    } else {
        zpm::outl(' ');
        zpm::outl(zpm::color::bold_green, "zpm-cde is up to date!", zpm::color::reset);
        return 0;
    }

    //performs zpm-cde upgrade
    upgrade();

    //update succes / fail
    endscreen();

    return 0;
}