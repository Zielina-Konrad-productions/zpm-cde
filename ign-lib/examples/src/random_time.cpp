#include <ign.hpp>

int main() {
    ign::println("Rolling dice...");
    ign::sleep_ms(250);

    const int roll = ign::random::integer(1, 6);
    ign::println("roll: ", roll);

    const std::string id = ign::random::hex(8);
    ign::println("id: ", id);

    if (ign::random::chance(0.5)) {
        ign::println("chance: yes");
    } else {
        ign::println("chance: no");
    }

    ign::sleep_sec(0.25);
    return 0;
}
