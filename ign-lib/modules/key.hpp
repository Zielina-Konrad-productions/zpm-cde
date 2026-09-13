#pragma once

#include <cerrno>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace ign::key {

// poll() returns this value when no key is waiting.
inline constexpr int no_key = -1;

namespace detail {

inline constexpr std::size_t max_buffered_keys = 256;

inline std::string multichar_literal_to_string(int value) {
    const auto bytes = static_cast<unsigned int>(value);
    std::string result;

    for (int shift = static_cast<int>(sizeof(int) * 8 - 8); shift >= 0; shift -= 8) {
        const auto byte = static_cast<unsigned char>((bytes >> shift) & 0xffU);
        if (byte != 0) {
            result.push_back(static_cast<char>(byte));
        }
    }

    return result;
}

inline bool consume_from_queue(std::queue<int>& keys, int expected) {
    std::queue<int> remaining;
    bool found = false;

    while (!keys.empty()) {
        const int key = keys.front();
        keys.pop();

        if (!found && key == expected) {
            found = true;
            continue;
        }

        remaining.push(key);
    }

    keys.swap(remaining);
    return found;
}

inline void push_limited(std::queue<int>& keys, int key) {
    while (keys.size() >= max_buffered_keys) {
        keys.pop();
    }

    keys.push(key);
}

#if !defined(_WIN32)
class key_reader {
public:
    key_reader() = default;
    key_reader(const key_reader&) = delete;
    key_reader& operator=(const key_reader&) = delete;

    ~key_reader() {
        stop();
    }

    int poll() {
        if (!start()) return no_key;

        std::lock_guard<std::mutex> lock(mutex_);
        if (keys_.empty()) return no_key;

        const int key = keys_.front();
        keys_.pop();
        return key;
    }

    bool consume(int expected) {
        if (!start()) return false;

        std::lock_guard<std::mutex> lock(mutex_);
        return consume_from_queue(keys_, expected);
    }

    int read() {
        const bool was_running = running_;
        if (!start()) return no_key;

        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [this] {
            return !keys_.empty() || !running_;
        });

        if (keys_.empty()) return no_key;

        const int key = keys_.front();
        keys_.pop();
        lock.unlock();

        if (!was_running) {
            stop();
        }

        return key;
    }

    void stop() {
        if (!running_) {
            restore_terminal();
            return;
        }

        running_ = false;
        ready_.notify_all();

        if (worker_.joinable()) {
            worker_.join();
        }

        restore_terminal();
        clear_queue();
    }

private:
    bool start() {
        if (running_) return true;

        if (!saved_ && ::tcgetattr(STDIN_FILENO, &original_) != 0) return false;
        saved_ = true;

        termios raw = original_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;

        if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;

        running_ = true;
        active_ = true;
        worker_ = std::thread([this] {
            run();
        });

        return true;
    }

    void run() {
        while (running_) {
            const int key = read_byte();
            if (key == no_key) continue;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                push_limited(keys_, key);
            }

            ready_.notify_one();
        }
    }

    int read_byte() {
        unsigned char key = 0;
        ssize_t result = 0;

        do {
            result = ::read(STDIN_FILENO, &key, 1);
        } while (result < 0 && errno == EINTR);

        return result == 1 ? static_cast<int>(key) : no_key;
    }

    void restore_terminal() {
        if (active_ && saved_) {
            ::tcsetattr(STDIN_FILENO, TCSANOW, &original_);
        }

        active_ = false;
    }

    void clear_queue() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<int> empty;
        keys_.swap(empty);
    }

    termios original_{};
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<int> keys_;
    bool saved_ = false;
    bool active_ = false;
};

inline key_reader& reader() {
    static key_reader state;
    return state;
}
#else
inline std::queue<int>& cached_keys() {
    static std::queue<int> keys;
    return keys;
}

inline int read_cached_key() {
    auto& keys = cached_keys();

    if (!keys.empty()) {
        const int key = keys.front();
        keys.pop();
        return key;
    }

    return ::_getch();
}

inline int poll_cached_key() {
    auto& keys = cached_keys();

    if (!keys.empty()) {
        const int key = keys.front();
        keys.pop();
        return key;
    }

    return ::_kbhit() ? ::_getch() : no_key;
}

inline bool consume_cached_key(int expected) {
    auto& keys = cached_keys();

    while (::_kbhit()) {
        push_limited(keys, ::_getch());
    }

    return consume_from_queue(keys, expected);
}
#endif

inline std::string& sequence_history() {
    static std::string history;
    return history;
}

inline std::mutex& sequence_mutex() {
    static std::mutex mutex;
    return mutex;
}

inline void remember_sequence_key(int key) {
    if (key == no_key) return;

    std::lock_guard<std::mutex> lock(sequence_mutex());
    auto& history = sequence_history();
    history.push_back(static_cast<char>(key));

    if (history.size() > max_buffered_keys) {
        history.erase(0, history.size() - max_buffered_keys);
    }
}

inline bool has_sequence(std::string_view expected) {
    std::lock_guard<std::mutex> lock(sequence_mutex());
    auto& history = sequence_history();
    const std::string text(expected);
    const std::size_t position = history.find(text);

    if (position == std::string::npos) {
        return false;
    }

    history.erase(0, position + text.size());
    return true;
}

inline void clear_sequence_history() {
    std::lock_guard<std::mutex> lock(sequence_mutex());
    sequence_history().clear();
}

} // namespace detail

// Waits for one key and returns it immediately, without requiring Enter.
inline int read() {
#if defined(_WIN32)
    return detail::read_cached_key();
#else
    return detail::reader().read();
#endif
}

// Waits for one key and returns it. Same as read(), but clearer in conditions.
inline int wait() {
    return read();
}

// Waits until the expected key is pressed.
inline bool wait(char expected) {
    while (true) {
        const int current = read();
        if (current == no_key) return false;
        if (current == static_cast<unsigned char>(expected)) return true;
    }
}

// Returns the next waiting key without blocking, or no_key when there is none.
inline int poll() {
#if defined(_WIN32)
    return detail::poll_cached_key();
#else
    return detail::reader().poll();
#endif
}

// Non-blocking check for a specific key. Other waiting keys are kept available.
inline bool is_pressed(char expected) {
#if defined(_WIN32)
    return detail::consume_cached_key(static_cast<unsigned char>(expected));
#else
    return detail::reader().consume(static_cast<unsigned char>(expected));
#endif
}

inline bool pressed(char expected) {
    return is_pressed(expected);
}

// Restores the terminal after using poll() or is_pressed().
inline void restore() {
#if !defined(_WIN32)
    detail::reader().stop();
#endif
    detail::clear_sequence_history();
}

// Non-blocking shortcut for recognizing a typed text sequence.
inline bool sequence_entered(std::string_view expected) {
    if (expected.empty()) return false;

    while (true) {
        const int key = poll();
        if (key == no_key) break;

        detail::remember_sequence_key(key);
    }

    return detail::has_sequence(expected);
}

// Recognizes a sequence from keys supplied to push(), one key at a time.
class sequence {
public:
    explicit sequence(const char* expected)
        : expected_(expected == nullptr ? "" : expected) {}

    explicit sequence(const std::string& expected)
        : expected_(expected) {}

    explicit sequence(std::string&& expected)
        : expected_(std::move(expected)) {}

    explicit sequence(std::string_view expected)
        : expected_(expected) {}

    explicit sequence(char expected)
        : expected_(1, expected) {}

    explicit sequence(int expected)
        : expected_(detail::multichar_literal_to_string(expected)) {}

    explicit operator bool() const {
        return sequence_entered(expected_);
    }

    bool push(int key) {
        if (key == no_key || expected_.empty()) return false;

        entered_.push_back(static_cast<char>(key));
        if (entered_.size() > expected_.size()) {
            entered_.erase(0, entered_.size() - expected_.size());
        }

        if (entered_ != expected_) return false;

        entered_.clear();
        return true;
    }

    void reset() {
        entered_.clear();
    }

private:
    std::string expected_;
    std::string entered_;
};

} // namespace ign::key
