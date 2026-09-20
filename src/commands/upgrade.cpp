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

    //variables
    bool upgradeavialable = false;

    //program arguments
    bool help = false;
    bool version = false;
    bool yes = false;

    void helpmessage() {

    }

    void versionmessage() {

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
    zpm::out("zpm-cde installed version: ");  zpm::common::versioncheck("../version.txt"); zpm::outl('.');
    zpm::out("zpm-cde latest version: "); std::string latest = printlatestversion(); zpm::outl('.');
    zpm::outl(zpm::color::cyan, "----------------------------------------------------", zpm::color::reset);

    std::string local = zpm::common::versioncheck_string("../version.txt");

if (compareversions(latest, local) > 0) {
        upgradeavialable = true;
}

    return 0;
}