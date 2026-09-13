#pragma once

#include <chrono>
#include <thread>
#include <type_traits>

namespace ign {

template<typename Rep, typename Period>
inline void sleep(const std::chrono::duration<Rep, Period>& duration) {
    std::this_thread::sleep_for(duration);
}

template<
    typename Number,
    typename = std::enable_if_t<
        std::is_arithmetic_v<Number> &&
        !std::is_same_v<std::decay_t<Number>, bool>
    >
>
inline void sleep_ns(Number nanoseconds) {
    sleep(
        std::chrono::duration<double, std::nano>(
            static_cast<double>(nanoseconds)
        )
    );
}

template<
    typename Number,
    typename = std::enable_if_t<
        std::is_arithmetic_v<Number> &&
        !std::is_same_v<std::decay_t<Number>, bool>
    >
>
inline void sleep_us(Number microseconds) {
    sleep(
        std::chrono::duration<double, std::micro>(
            static_cast<double>(microseconds)
        )
    );
}

template<
    typename Number,
    typename = std::enable_if_t<
        std::is_arithmetic_v<Number> &&
        !std::is_same_v<std::decay_t<Number>, bool>
    >
>
inline void sleep_ms(Number milliseconds) {
    sleep(
        std::chrono::duration<double, std::milli>(
            static_cast<double>(milliseconds)
        )
    );
}

template<
    typename Number,
    typename = std::enable_if_t<
        std::is_arithmetic_v<Number> &&
        !std::is_same_v<std::decay_t<Number>, bool>
    >
>
inline void sleep_sec(Number seconds) {
    sleep(
        std::chrono::duration<double>(
            static_cast<double>(seconds)
        )
    );
}

template<
    typename Number,
    typename = std::enable_if_t<
        std::is_arithmetic_v<Number> &&
        !std::is_same_v<std::decay_t<Number>, bool>
    >
>
inline void sleep_min(Number minutes) {
    sleep(
        std::chrono::duration<double, std::ratio<60>>(
            static_cast<double>(minutes)
        )
    );
}

template<
    typename Number,
    typename = std::enable_if_t<
        std::is_arithmetic_v<Number> &&
        !std::is_same_v<std::decay_t<Number>, bool>
    >
>
inline void sleep(Number milliseconds) {
    sleep_ms(milliseconds);
}

} // namespace ign
