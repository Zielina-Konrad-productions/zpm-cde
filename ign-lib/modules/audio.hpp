#pragma once
#include "path.hpp"
#include "status.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "../third_party/subprocess.h/subprocess.h"

#if !defined(IGN_AUDIO_DISABLE_SFML) && \
    defined(IGN_AUDIO_ENABLE_INPROCESS_SFML)
#if defined(__has_include)
#if __has_include("../third_party/SFML/include/SFML/Audio.hpp")
#define IGN_AUDIO_HAS_SFML 1
#include "../third_party/SFML/include/SFML/Audio.hpp"
#elif __has_include(<SFML/Audio.hpp>)
#define IGN_AUDIO_HAS_SFML 1
#include <SFML/Audio.hpp>
#endif
#endif
#endif

#if !defined(IGN_AUDIO_HAS_SFML)
#define IGN_AUDIO_HAS_SFML 0
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ign::audio {

    namespace detail {

        inline constexpr float min_volume = 0.0f;
        inline constexpr float max_volume = 100.0f;

        struct backend {
            std::string name;
            std::vector<std::string> command;
        };

        inline float clamp_volume(float value) noexcept {
            if (value < min_volume) return min_volume;
            if (value > max_volume) return max_volume;
            return value;
        }

        inline std::mutex& settings_mutex() {
            static std::mutex mutex;
            return mutex;
        }

        inline float& master_volume_storage() {
            static float value = max_volume;
            return value;
        }

        inline bool& muted_storage() {
            static bool value = false;
            return value;
        }

        inline float master_volume() {
            std::lock_guard<std::mutex> lock(settings_mutex());
            return master_volume_storage();
        }

        inline void set_master_volume(float value) {
            std::lock_guard<std::mutex> lock(settings_mutex());
            master_volume_storage() = clamp_volume(value);
        }

        inline bool muted() {
            std::lock_guard<std::mutex> lock(settings_mutex());
            return muted_storage();
        }

        inline void set_muted(bool value) {
            std::lock_guard<std::mutex> lock(settings_mutex());
            muted_storage() = value;
        }

        inline float effective_volume(float local_volume) {
            std::lock_guard<std::mutex> lock(settings_mutex());
            if (muted_storage()) return min_volume;

            return clamp_volume(local_volume) *
                (master_volume_storage() / max_volume);
        }

        inline ign::status error_result(const std::error_code& ec) {
            if (ec == std::errc::permission_denied ||
                ec == std::errc::operation_not_permitted) {
                return ign::status::permission_denied;
            }

            if (ec == std::errc::no_such_file_or_directory) {
                return ign::status::not_found;
            }

            return ign::status::error;
        }

        inline ign::status validate_audio_file(
            const std::filesystem::path& path) {
            std::error_code ec;

            if (!std::filesystem::exists(path, ec)) {
                return ec ? error_result(ec) : ign::status::not_found;
            }

            if (std::filesystem::is_directory(path, ec)) {
                return ec ? error_result(ec) : ign::status::error;
            }

            return ec ? error_result(ec) : ign::status::ok;
        }

#if IGN_AUDIO_HAS_SFML
        struct sfml_playback {
            std::shared_ptr<sf::Music> music;
            float local_volume = max_volume;
            bool looping = false;
        };

        inline std::mutex& sfml_mutex() {
            static std::mutex mutex;
            return mutex;
        }

        inline std::vector<std::shared_ptr<sfml_playback>>&
        sfml_playbacks() {
            static std::vector<std::shared_ptr<sfml_playback>> playbacks;
            return playbacks;
        }

        inline bool sfml_open_from_file(
            sf::Music& music,
            const std::filesystem::path& path) {
            return music.openFromFile(path.string());
        }

        inline bool sfml_status_playing(const sf::Music& music) {
#if defined(SFML_VERSION_MAJOR) && SFML_VERSION_MAJOR >= 3
            return music.getStatus() == sf::SoundSource::Status::Playing;
#else
            return music.getStatus() == sf::SoundSource::Playing;
#endif
        }

        inline bool sfml_status_active(const sf::Music& music) {
#if defined(SFML_VERSION_MAJOR) && SFML_VERSION_MAJOR >= 3
            return music.getStatus() == sf::SoundSource::Status::Playing ||
                music.getStatus() == sf::SoundSource::Status::Paused;
#else
            return music.getStatus() == sf::SoundSource::Playing ||
                music.getStatus() == sf::SoundSource::Paused;
#endif
        }

        inline void sfml_set_looping(sf::Music& music, bool loop) {
#if defined(SFML_VERSION_MAJOR) && SFML_VERSION_MAJOR >= 3
            music.setLooping(loop);
#else
            music.setLoop(loop);
#endif
        }

        inline void remove_sfml_playback(
            const std::shared_ptr<sfml_playback>& playback) {
            std::lock_guard<std::mutex> lock(sfml_mutex());
            std::vector<std::shared_ptr<sfml_playback>>& playbacks =
                sfml_playbacks();

            playbacks.erase(
                std::remove(playbacks.begin(), playbacks.end(), playback),
                playbacks.end()
            );
        }

        inline void cleanup_sfml_playbacks() {
            std::lock_guard<std::mutex> lock(sfml_mutex());
            std::vector<std::shared_ptr<sfml_playback>>& playbacks =
                sfml_playbacks();

            playbacks.erase(
                std::remove_if(
                    playbacks.begin(),
                    playbacks.end(),
                    [](const std::shared_ptr<sfml_playback>& playback) {
                        return !playback ||
                            !playback->music ||
                            !sfml_status_active(*playback->music);
                    }
                ),
                playbacks.end()
            );
        }

        inline void refresh_sfml_volumes() {
            std::lock_guard<std::mutex> lock(sfml_mutex());

            for (const std::shared_ptr<sfml_playback>& playback
                 : sfml_playbacks()) {
                if (playback && playback->music) {
                    playback->music->setVolume(
                        effective_volume(playback->local_volume));
                }
            }
        }

        inline ign::status play_sfml(
            const std::filesystem::path& path,
            float volume) {
            sf::Music music;

            if (!sfml_open_from_file(music, path)) {
                return ign::status::error;
            }

            music.setVolume(effective_volume(volume));
            music.play();

            while (sfml_status_playing(music)) {
                music.setVolume(effective_volume(volume));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            return ign::status::ok;
        }

        inline ign::status play_sfml_async(
            const std::filesystem::path& path,
            float volume) {
            cleanup_sfml_playbacks();

            auto playback = std::make_shared<sfml_playback>();
            playback->music = std::make_shared<sf::Music>();
            playback->local_volume = clamp_volume(volume);
            playback->looping = false;

            if (!sfml_open_from_file(*playback->music, path)) {
                return ign::status::error;
            }

            playback->music->setVolume(effective_volume(volume));
            playback->music->play();

            {
                std::lock_guard<std::mutex> lock(sfml_mutex());
                sfml_playbacks().push_back(playback);
            }

            try {
                std::thread([playback] {
                    while (playback->music &&
                           sfml_status_active(*playback->music)) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(25));
                    }

                    remove_sfml_playback(playback);
                }).detach();
            } catch (...) {
                if (playback->music) playback->music->stop();
                remove_sfml_playback(playback);
                return ign::status::error;
            }

            return ign::status::ok;
        }

        inline ign::status play_sfml_loop(
            const std::filesystem::path& path,
            float volume) {
            cleanup_sfml_playbacks();

            auto playback = std::make_shared<sfml_playback>();
            playback->music = std::make_shared<sf::Music>();
            playback->local_volume = clamp_volume(volume);
            playback->looping = true;

            if (!sfml_open_from_file(*playback->music, path)) {
                return ign::status::error;
            }

            playback->music->setVolume(effective_volume(volume));
            sfml_set_looping(*playback->music, true);
            playback->music->play();

            {
                std::lock_guard<std::mutex> lock(sfml_mutex());
                sfml_playbacks().push_back(playback);
            }

            return ign::status::ok;
        }

        inline void stop_sfml(bool loops_only = false) {
            std::lock_guard<std::mutex> lock(sfml_mutex());

            std::vector<std::shared_ptr<sfml_playback>>& playbacks =
                sfml_playbacks();

            for (auto iterator = playbacks.begin();
                 iterator != playbacks.end();) {
                const std::shared_ptr<sfml_playback>& playback = *iterator;
                const bool should_stop =
                    playback && (!loops_only || playback->looping);

                if (should_stop && playback->music) {
                    playback->music->stop();
                }

                if (should_stop) {
                    iterator = playbacks.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }
#endif

        inline bool has_path_separator(const std::string& text) {
            return text.find('/') != std::string::npos ||
                text.find('\\') != std::string::npos;
        }

        inline char path_list_separator() {
#if defined(_WIN32)
            return ';';
#else
            return ':';
#endif
        }

        inline std::vector<std::filesystem::path> path_directories() {
            std::vector<std::filesystem::path> directories;

            const char* path_value = std::getenv("PATH");
            if (path_value == nullptr || *path_value == '\0') {
                return directories;
            }

            const std::string path_text(path_value);
            const char separator = path_list_separator();
            std::size_t start = 0;

            while (start <= path_text.size()) {
                const std::size_t end = path_text.find(separator, start);
                const std::string item = path_text.substr(
                    start,
                    end == std::string::npos
                        ? std::string::npos
                        : end - start
                );

                directories.emplace_back(item.empty() ? "." : item);

                if (end == std::string::npos) break;
                start = end + 1;
            }

            return directories;
        }

        inline bool executable_candidate_exists(
            const std::filesystem::path& path) {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec) || ec) return false;
            return !std::filesystem::is_directory(path, ec) && !ec;
        }

#if defined(_WIN32)
        inline std::vector<std::string> executable_extensions(
            const std::filesystem::path& program) {
            if (program.has_extension()) return {""};

            const char* pathext_value = std::getenv("PATHEXT");
            if (pathext_value == nullptr || *pathext_value == '\0') {
                return {".COM", ".EXE", ".BAT", ".CMD"};
            }

            std::vector<std::string> extensions;
            const std::string pathext(pathext_value);
            std::size_t start = 0;

            while (start <= pathext.size()) {
                const std::size_t end = pathext.find(';', start);
                const std::string item = pathext.substr(
                    start,
                    end == std::string::npos
                        ? std::string::npos
                        : end - start
                );

                if (!item.empty()) extensions.push_back(item);

                if (end == std::string::npos) break;
                start = end + 1;
            }

            return extensions.empty()
                ? std::vector<std::string>{".COM", ".EXE", ".BAT", ".CMD"}
                : extensions;
        }
#else
        inline std::vector<std::string> executable_extensions(
            const std::filesystem::path&) {
            return {""};
        }
#endif

        inline bool executable_exists(const std::string& program_name) {
            if (program_name.empty()) return false;

            const std::filesystem::path program(program_name);
            const std::vector<std::string> extensions =
                executable_extensions(program);

            if (program.is_absolute() || has_path_separator(program_name)) {
                for (const std::string& extension : extensions) {
                    if (executable_candidate_exists(program_name + extension)) {
                        return true;
                    }
                }
                return false;
            }

            for (const std::filesystem::path& directory : path_directories()) {
                for (const std::string& extension : extensions) {
                    if (executable_candidate_exists(
                            directory / (program_name + extension))) {
                        return true;
                    }
                }
            }

            return false;
        }

        inline std::filesystem::path sfml_helper_filename() {
#if defined(_WIN32)
            return "ign-sfml-audio-player.exe";
#else
            return "ign-sfml-audio-player";
#endif
        }

        inline std::vector<std::filesystem::path> sfml_helper_candidates() {
            std::vector<std::filesystem::path> candidates;

#if !defined(IGN_AUDIO_DISABLE_SFML)
            const std::filesystem::path module_path(__FILE__);
            const std::filesystem::path module_root =
                module_path.has_parent_path()
                    ? module_path.parent_path().parent_path()
                    : std::filesystem::path();

            if (!module_root.empty()) {
                candidates.push_back(
                    module_root /
                    "third_party" /
                    "SFML" /
                    "bin" /
                    sfml_helper_filename()
                );
                candidates.push_back(
                    module_root /
                    "third_party" /
                    "ign_sfml_audio_player" /
                    "bin" /
                    sfml_helper_filename()
                );
            }

            std::error_code ec;
            const std::filesystem::path current =
                std::filesystem::current_path(ec);

            if (!ec) {
                candidates.push_back(
                    current /
                    "ign-lib" /
                    "third_party" /
                    "SFML" /
                    "bin" /
                    sfml_helper_filename()
                );
                candidates.push_back(
                    current /
                    "third_party" /
                    "SFML" /
                    "bin" /
                    sfml_helper_filename()
                );
            }
#endif

            return candidates;
        }

        inline std::vector<backend> sfml_helper_backends() {
            std::vector<backend> backends;

            for (const std::filesystem::path& path
                 : sfml_helper_candidates()) {
                if (executable_candidate_exists(path)) {
                    backends.push_back({"sfml", {path.string()}});
                }
            }

            return backends;
        }

        inline std::vector<backend> default_backends() {
            std::vector<backend> backends = sfml_helper_backends();

#if defined(_WIN32)
            const std::vector<backend> platform_backends{
                {
                    "ffplay",
                    {"ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet"}
                }
            };
#elif defined(__APPLE__)
            const std::vector<backend> platform_backends{
                {"afplay", {"afplay"}},
                {
                    "ffplay",
                    {"ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet"}
                },
                {"play", {"play", "-q"}}
            };
#else
            const std::vector<backend> platform_backends{
                {"paplay", {"paplay"}},
                {"pw-play", {"pw-play"}},
                {
                    "ffplay",
                    {"ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet"}
                },
                {"aplay", {"aplay", "-q"}},
                {"play", {"play", "-q"}}
            };
#endif

            backends.insert(
                backends.end(),
                platform_backends.begin(),
                platform_backends.end()
            );

            return backends;
        }

        inline bool backend_available(const backend& backend) {
            return !backend.command.empty() &&
                executable_exists(backend.command.front());
        }

        inline bool backend_supports_volume(const backend& backend) {
            return backend.name == "sfml" ||
                backend.name == "afplay" ||
                backend.name == "ffplay" ||
                backend.name == "paplay" ||
                backend.name == "play";
        }

        inline bool backend_supports_loop(const backend& backend) {
            return backend.name == "sfml";
        }

        inline bool volume_needs_support(float volume) {
            return clamp_volume(volume) < max_volume;
        }

#if defined(_WIN32)
        using play_sound_w_function = BOOL (WINAPI*)(
            LPCWSTR pszSound,
            HMODULE hmod,
            DWORD fdwSound
        );

        inline HMODULE winmm_library() {
            static HMODULE library = LoadLibraryW(L"winmm.dll");
            return library;
        }

        inline play_sound_w_function winmm_play_sound() {
            HMODULE library = winmm_library();
            if (library == nullptr) return nullptr;

            static play_sound_w_function function =
                reinterpret_cast<play_sound_w_function>(
                    GetProcAddress(library, "PlaySoundW"));

            return function;
        }

        inline bool native_backend_available() {
            return winmm_play_sound() != nullptr;
        }

        inline ign::status play_native_windows(
            const std::filesystem::path& path,
            bool async) {
            constexpr DWORD sound_sync = 0x0000;
            constexpr DWORD sound_async = 0x0001;
            constexpr DWORD sound_nodefault = 0x0002;
            constexpr DWORD sound_filename = 0x00020000;

            const play_sound_w_function play_sound = winmm_play_sound();
            if (play_sound == nullptr) return ign::status::error;

            const std::wstring wide_path = path.wstring();
            const DWORD flags =
                sound_filename |
                sound_nodefault |
                (async ? sound_async : sound_sync);

            const BOOL played = play_sound(wide_path.c_str(), nullptr, flags);

            return played ? ign::status::ok : ign::status::error;
        }
#endif

        inline backend first_available_backend() {
            for (const backend& item : default_backends()) {
                if (backend_available(item)) return item;
            }

            return {};
        }

        inline std::string volume_factor_argument(float volume) {
            std::ostringstream stream;
            stream << (clamp_volume(volume) / max_volume);
            return stream.str();
        }

        inline std::string pulse_volume_argument(float volume) {
            const unsigned long value = static_cast<unsigned long>(
                (clamp_volume(volume) / max_volume) * 65536.0f);

            return std::to_string(value);
        }

        inline void append_volume_arguments(
            std::vector<std::string>& arguments,
            const backend& backend,
            float volume) {
            if (!backend_supports_volume(backend)) return;

            if (backend.name == "sfml") {
                arguments.push_back("--volume");
                arguments.push_back(
                    std::to_string(static_cast<int>(clamp_volume(volume))));
                return;
            }

            if (backend.name == "afplay") {
                arguments.push_back("-v");
                arguments.push_back(volume_factor_argument(volume));
                return;
            }

            if (backend.name == "ffplay") {
                arguments.push_back("-volume");
                arguments.push_back(
                    std::to_string(static_cast<int>(clamp_volume(volume))));
                return;
            }

            if (backend.name == "paplay") {
                arguments.push_back("--volume=" + pulse_volume_argument(volume));
                return;
            }

            if (backend.name == "play") {
                arguments.push_back("-v");
                arguments.push_back(volume_factor_argument(volume));
            }
        }

        inline void append_loop_arguments(
            std::vector<std::string>& arguments,
            const backend& backend,
            bool loop) {
            if (!loop || !backend_supports_loop(backend)) return;

            arguments.push_back("--loop");
        }

        inline std::vector<std::string> arguments_for(
            const backend& backend,
            const std::filesystem::path& path,
            float volume,
            bool loop = false) {
            std::vector<std::string> arguments = backend.command;
            append_volume_arguments(arguments, backend, volume);
            append_loop_arguments(arguments, backend, loop);
            arguments.push_back(path.string());
            return arguments;
        }

        inline std::vector<const char*> make_argv(
            const std::vector<std::string>& arguments) {
            std::vector<const char*> argv;
            argv.reserve(arguments.size() + 1);

            for (const std::string& argument : arguments) {
                argv.push_back(argument.c_str());
            }
            argv.push_back(nullptr);

            return argv;
        }

        inline ign::status process_error_result(int result) {
            if (result == subprocess_error_permission_denied) {
                return ign::status::permission_denied;
            }

            if (result == subprocess_error_not_found) {
                return ign::status::not_found;
            }

            return ign::status::error;
        }

        inline ign::status start_process(
            const std::vector<std::string>& arguments,
            subprocess_s& process) {
            if (arguments.empty() || arguments.front().empty()) {
                return ign::status::not_found;
            }

            std::vector<const char*> argv = make_argv(arguments);
            const int result = subprocess_create(
                argv.data(),
                subprocess_option_search_user_path |
                    subprocess_option_inherit_environment |
                    subprocess_option_combined_stdout_stderr |
                    subprocess_option_enable_async |
                    subprocess_option_enable_async_no_wait |
                    subprocess_option_no_window,
                &process
            );

            if (result != subprocess_error_success) {
                return process_error_result(result);
            }

            if (process.stdin_file != nullptr) {
                std::fclose(process.stdin_file);
                process.stdin_file = nullptr;
            }

            return ign::status::ok;
        }

        inline void drain_output(subprocess_s& process) {
            std::array<char, 4096> buffer{};

            while (subprocess_read_stdout(
                       &process,
                       buffer.data(),
                       static_cast<unsigned>(buffer.size())) > 0) {
            }
        }

        inline ign::status wait_for_process(subprocess_s& process) {
            int alive_result = 0;

            do {
                drain_output(process);
                alive_result = subprocess_alive(&process);
                if (alive_result > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(10));
                }
            } while (alive_result > 0);

            drain_output(process);

            int return_code = 0;
            const int join_result = subprocess_join(&process, &return_code);
            const int destroy_result = subprocess_destroy(&process);

            if (alive_result < 0 ||
                join_result != subprocess_error_success ||
                destroy_result != subprocess_error_success) {
                return ign::status::error;
            }

            return return_code == 0 ? ign::status::ok : ign::status::error;
        }

        inline void terminate_process(subprocess_s& process) {
            subprocess_terminate(&process);
            subprocess_join(&process, nullptr);
            subprocess_destroy(&process);
        }

        inline std::mutex& loop_process_mutex() {
            static std::mutex mutex;
            return mutex;
        }

        inline std::vector<std::shared_ptr<subprocess_s>>& loop_processes() {
            static std::vector<std::shared_ptr<subprocess_s>> processes;
            return processes;
        }

        inline void cleanup_loop_processes() {
            std::lock_guard<std::mutex> lock(loop_process_mutex());
            std::vector<std::shared_ptr<subprocess_s>>& processes =
                loop_processes();

            for (auto iterator = processes.begin();
                 iterator != processes.end();) {
                std::shared_ptr<subprocess_s>& process = *iterator;

                if (!process) {
                    iterator = processes.erase(iterator);
                    continue;
                }

                const int alive = subprocess_alive(process.get());
                if (alive > 0) {
                    ++iterator;
                    continue;
                }

                wait_for_process(*process);
                iterator = processes.erase(iterator);
            }
        }

        inline void stop_loop_processes() {
            std::vector<std::shared_ptr<subprocess_s>> processes;

            {
                std::lock_guard<std::mutex> lock(loop_process_mutex());
                processes.swap(loop_processes());
            }

            for (const std::shared_ptr<subprocess_s>& process : processes) {
                if (!process) continue;

                const int alive = subprocess_alive(process.get());
                if (alive > 0) {
                    terminate_process(*process);
                } else {
                    wait_for_process(*process);
                }
            }
        }

        inline ign::status run_player(
            const backend& backend,
            const std::filesystem::path& path,
            float volume) {
            subprocess_s process{};
            const ign::status start_result =
                start_process(arguments_for(backend, path, volume), process);

            if (start_result != ign::status::ok) return start_result;
            return wait_for_process(process);
        }

        inline ign::status run_player_async(
            const backend& backend,
            const std::filesystem::path& path,
            float volume) {
            auto process = std::make_shared<subprocess_s>();
            const ign::status start_result =
                start_process(arguments_for(backend, path, volume), *process);

            if (start_result != ign::status::ok) return start_result;

            try {
                std::thread([process] {
                    wait_for_process(*process);
                }).detach();
            } catch (...) {
                terminate_process(*process);
                return ign::status::error;
            }

            return ign::status::ok;
        }

        inline ign::status run_player_loop(
            const backend& backend,
            const std::filesystem::path& path,
            float volume) {
            cleanup_loop_processes();

            auto process = std::make_shared<subprocess_s>();
            const ign::status start_result =
                start_process(arguments_for(backend, path, volume, true),
                              *process);

            if (start_result != ign::status::ok) return start_result;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            drain_output(*process);

            const int alive = subprocess_alive(process.get());
            if (alive <= 0) {
                const ign::status wait_result = wait_for_process(*process);
                return wait_result == ign::status::ok
                    ? ign::status::error
                    : wait_result;
            }

            {
                std::lock_guard<std::mutex> lock(loop_process_mutex());
                loop_processes().push_back(process);
            }

            return ign::status::ok;
        }

        inline ign::status play_impl(
            const std::filesystem::path& path,
            bool async,
            float volume) {
            const std::filesystem::path fullpath =
                ign::detail::expand_user_path(path);

            const ign::status file_result = validate_audio_file(fullpath);
            if (file_result != ign::status::ok) return file_result;

            const float output_volume = effective_volume(volume);
            if (output_volume <= min_volume) return ign::status::ok;

            bool found_backend = false;
            ign::status result = ign::status::error;

#if IGN_AUDIO_HAS_SFML
            found_backend = true;

            const ign::status sfml_result = async
                ? play_sfml_async(fullpath, volume)
                : play_sfml(fullpath, volume);

            if (sfml_result == ign::status::ok) return sfml_result;
            result = ign::detail::merge_status(result, sfml_result);
#endif

#if defined(_WIN32)
            if (!volume_needs_support(output_volume) &&
                native_backend_available()) {
                found_backend = true;

                const ign::status current =
                    play_native_windows(fullpath, async);

                if (current == ign::status::ok) return current;
                result = ign::detail::merge_status(result, current);
            }
#endif

            for (const backend& item : default_backends()) {
                if (!backend_available(item)) continue;
                if (volume_needs_support(output_volume) &&
                    !backend_supports_volume(item)) {
                    continue;
                }

                found_backend = true;

                const ign::status current = async
                    ? run_player_async(item, fullpath, output_volume)
                    : run_player(item, fullpath, output_volume);

                if (current == ign::status::ok) return current;
                result = ign::detail::merge_status(result, current);
            }

            return found_backend ? result : ign::status::error;
        }

        inline ign::status play_loop_impl(
            const std::filesystem::path& path,
            float volume) {
            const std::filesystem::path fullpath =
                ign::detail::expand_user_path(path);

            const ign::status file_result = validate_audio_file(fullpath);
            if (file_result != ign::status::ok) return file_result;

            const float output_volume = effective_volume(volume);
            if (output_volume <= min_volume) return ign::status::ok;

            bool found_backend = false;
            ign::status result = ign::status::error;

#if IGN_AUDIO_HAS_SFML
            found_backend = true;

            const ign::status sfml_result =
                play_sfml_loop(fullpath, volume);

            if (sfml_result == ign::status::ok) return sfml_result;
            result = ign::detail::merge_status(result, sfml_result);
#endif

            for (const backend& item : default_backends()) {
                if (!backend_available(item)) continue;
                if (!backend_supports_loop(item)) continue;

                found_backend = true;

                const ign::status current =
                    run_player_loop(item, fullpath, output_volume);

                if (current == ign::status::ok) return current;
                result = ign::detail::merge_status(result, current);
            }

            return found_backend ? result : ign::status::error;
        }

    }

    // Returns true when ign-lib can find a supported audio backend.
    inline bool available() {
#if IGN_AUDIO_HAS_SFML
        return true;
#else
#if defined(_WIN32)
        if (detail::native_backend_available()) return true;
#endif

        return detail::backend_available(detail::first_available_backend());
#endif
    }

    // Returns the selected backend name, for example "sfml" or "paplay".
    inline std::string player() {
#if IGN_AUDIO_HAS_SFML
        return "sfml";
#else
#if defined(_WIN32)
        if (detail::native_backend_available()) return "winmm";
#endif

        return detail::first_available_backend().name;
#endif
    }

    // Sets the master audio volume. Values are clamped to 0..100.
    inline void set_volume(float volume) {
        detail::set_master_volume(volume);
#if IGN_AUDIO_HAS_SFML
        detail::refresh_sfml_volumes();
#endif
    }

    // Returns the master audio volume in the 0..100 range.
    inline float get_volume() {
        return detail::master_volume();
    }

    // Short alias for get_volume().
    inline float volume() {
        return get_volume();
    }

    // Enables or disables global muting.
    inline void set_muted(bool muted) {
        detail::set_muted(muted);
#if IGN_AUDIO_HAS_SFML
        detail::refresh_sfml_volumes();
#endif
    }

    inline bool is_muted() {
        return detail::muted();
    }

    inline void mute() {
        set_muted(true);
    }

    inline void unmute() {
        set_muted(false);
    }

    // Stops playback started by play_loop().
    inline void stop_loop() {
        detail::stop_loop_processes();
#if IGN_AUDIO_HAS_SFML
        detail::stop_sfml(true);
#endif
    }

    // Stops currently tracked SFML playback. Process fallback playback keeps
    // its original fire-and-forget behavior, except play_loop() processes.
    inline void stop_all() {
        detail::stop_loop_processes();
#if IGN_AUDIO_HAS_SFML
        detail::stop_sfml();
#endif
    }

    // Plays an audio file and waits until it finishes.
    // Paths beginning with ~ are expanded to the current user's home directory.
    inline ign::status play(const std::filesystem::path& path) {
        return detail::play_impl(path, false, detail::max_volume);
    }

    // Plays an audio file at a per-call volume in the 0..100 range.
    inline ign::status play(
        const std::filesystem::path& path,
        float volume) {
        return detail::play_impl(path, false, volume);
    }

    // Starts playback in the background and returns after the player starts.
    inline ign::status play_async(const std::filesystem::path& path) {
        return detail::play_impl(path, true, detail::max_volume);
    }

    // Starts playback in the background with a per-call volume in 0..100.
    inline ign::status play_async(
        const std::filesystem::path& path,
        float volume) {
        return detail::play_impl(path, true, volume);
    }

    // Starts looped playback in the background. With the bundled SFML backend
    // the loop is handled by SFML, so it does not restart the file manually.
    inline ign::status play_loop(const std::filesystem::path& path) {
        return detail::play_loop_impl(path, detail::max_volume);
    }

    // Starts looped playback in the background with a per-call volume in 0..100.
    inline ign::status play_loop(
        const std::filesystem::path& path,
        float volume) {
        return detail::play_loop_impl(path, volume);
    }

}
