#include <SFML/Audio.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

float clamp_volume(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 100.0f) return 100.0f;
    return value;
}

bool is_playing(const sf::Music& music) {
#if defined(SFML_VERSION_MAJOR) && SFML_VERSION_MAJOR >= 3
    return music.getStatus() == sf::SoundSource::Status::Playing;
#else
    return music.getStatus() == sf::SoundSource::Playing;
#endif
}

void set_looping(sf::Music& music, bool loop) {
#if defined(SFML_VERSION_MAJOR) && SFML_VERSION_MAJOR >= 3
    music.setLooping(loop);
#else
    music.setLoop(loop);
#endif
}

bool read_float(const std::string& text, float& value) {
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);

    if (end == text.c_str() || *end != '\0') return false;

    value = parsed;
    return true;
}

void usage() {
    std::cerr
        << "Usage: ign-sfml-audio-player [--loop] "
           "[--volume 0..100] audio-file\n";
}

} // namespace

int main(int argc, char** argv) {
    float volume = 100.0f;
    bool loop = false;
    std::filesystem::path path;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index] == nullptr
            ? std::string()
            : std::string(argv[index]);

        if (argument == "--help" || argument == "-h") {
            usage();
            return 0;
        }

        if (argument == "--loop" || argument == "-l") {
            loop = true;
            continue;
        }

        if (argument == "--volume" || argument == "-v") {
            if (index + 1 >= argc || !read_float(argv[++index], volume)) {
                usage();
                return 1;
            }

            continue;
        }

        if (argument.rfind("--volume=", 0) == 0) {
            if (!read_float(argument.substr(9), volume)) {
                usage();
                return 1;
            }

            continue;
        }

        if (path.empty()) {
            path = argument;
            continue;
        }

        usage();
        return 1;
    }

    if (path.empty()) {
        usage();
        return 1;
    }

    try {
        sf::Music music;

        if (!music.openFromFile(path)) {
            return 1;
        }

        music.setVolume(clamp_volume(volume));
        set_looping(music, loop);
        music.play();

        while (is_playing(music)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
