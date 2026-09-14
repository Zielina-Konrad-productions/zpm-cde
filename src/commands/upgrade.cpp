#include "upgrade.hpp"
#include <iostream>
#include "../../zpm-lib/zpm.hpp"
#include "../../ign-lib/ign.hpp"
#include <cstdlib>


namespace {

    //program arguments
    bool help = false;
    bool version = false;
    bool yes = false;

    void helpmessage() {

    }

    void versionmessage() {

    }

    

    void printlatestversion() {
        ign::run_shell("curl -s https://api.github.com/repos/Zielina-Konrad-productions/zpm-cde/releases/latest | grep '\"tag_name\":' | cut -d'\"' -f4");
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
    zpm::out("zpm-cde installed version: ");  zpm::common::versioncheck("version.txt"); zpm::outl('.');
    zpm::out("zpm-cde latest version: "); printlatestversion(); zpm::outl('.');


    return 0;
}