#include <ign.hpp>

int main() {
    int code = ign::run({"printf", "hello from ign::run\n"});
    if (code != 0) {
        ign::println_error_red("printf failed with code ", code);
        return code;
    }

    code = ign::run_shell("echo hello from ign::run_shell");
    if (code != 0) {
        ign::println_error_red("shell command failed with code ", code);
        return code;
    }

    return 0;
}
