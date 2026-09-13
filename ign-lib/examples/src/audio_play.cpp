#include <ign.hpp>

int main(int argc, char** argv) {
    if (argc < 2) {
        ign::println("Usage: audio_play path/to/sound.wav");
        return 1;
    }

    if (!ign::audio::available()) {
        ign::println("No supported audio player found.");
        return 1;
    }

    ign::println("Using audio player: ", ign::audio::player());

    const ign::status result = ign::audio::play(argv[1]);
    if (result != ign::status::ok) {
        ign::println("Could not play audio file.");
        return 1;
    }

    return 0;
}
