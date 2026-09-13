#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ign::random {

namespace detail {

template<typename T>
using clean_t = std::decay_t<T>;

template<typename T>
inline constexpr bool is_number_v =
    std::is_arithmetic_v<clean_t<T>> &&
    !std::is_same_v<clean_t<T>, bool>;

template<typename T>
using enable_number_t = std::enable_if_t<is_number_v<T>, int>;

template<typename T>
using enable_integer_t = std::enable_if_t<
    std::is_integral_v<clean_t<T>> &&
    !std::is_same_v<clean_t<T>, bool>,
    int>;

template<typename T>
using enable_real_t = std::enable_if_t<
    std::is_floating_point_v<clean_t<T>>,
    int>;

inline std::mt19937_64 make_engine() {
    std::array<unsigned int, 8> seed_data{};
    const auto now = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count());

    seed_data[0] = static_cast<unsigned int>(now);
    seed_data[1] = static_cast<unsigned int>(now >> 32);
    seed_data[2] = static_cast<unsigned int>(
        reinterpret_cast<std::uintptr_t>(&seed_data));
    seed_data[3] = static_cast<unsigned int>(
        reinterpret_cast<std::uintptr_t>(&seed_data) >> 32);

    try {
        std::random_device device;
        for (std::size_t index = 4; index < seed_data.size(); ++index) {
            seed_data[index] = device();
        }
    } catch (...) {
        for (std::size_t index = 4; index < seed_data.size(); ++index) {
            seed_data[index] =
                seed_data[index - 4] * 1664525U + 1013904223U;
        }
    }

    std::seed_seq seed(seed_data.begin(), seed_data.end());
    return std::mt19937_64(seed);
}

inline std::mt19937_64& generator() {
    static std::mt19937_64 engine = make_engine();
    return engine;
}

inline double normalized_probability(double probability) {
    if (!(probability > 0.0)) return 0.0;
    if (probability >= 1.0) return 1.0;
    return probability;
}

inline constexpr std::string_view default_alphabet() {
    return "abcdefghijklmnopqrstuvwxyz"
           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
           "0123456789";
}

inline constexpr std::string_view hex_alphabet() {
    return "0123456789abcdef";
}

} // namespace detail

// Returns the shared pseudo-random engine used by ign::random.
inline std::mt19937_64& engine() {
    return detail::generator();
}

// Sets a deterministic seed for reproducible random values.
inline void seed(std::uint64_t value) {
    engine().seed(value);
}

// Seeds the shared engine with a custom std::seed_seq.
inline void seed(std::seed_seq& sequence) {
    engine().seed(sequence);
}

// Seeds the shared engine again with entropy from the system and clock.
inline void reseed() {
    engine() = detail::make_engine();
}

// Returns a random integer in the inclusive range [min, max].
template<typename Integer, detail::enable_integer_t<Integer> = 0>
inline Integer integer(Integer min, Integer max) {
    using distribution_type = std::conditional_t<
        std::is_signed_v<detail::clean_t<Integer>>,
        long long,
        unsigned long long>;

    auto low = static_cast<distribution_type>(min);
    auto high = static_cast<distribution_type>(max);
    if (high < low) std::swap(low, high);

    std::uniform_int_distribution<distribution_type> distribution(low, high);
    return static_cast<Integer>(distribution(engine()));
}

// Returns a random integer in the inclusive range [0, max].
template<typename Integer, detail::enable_integer_t<Integer> = 0>
inline Integer integer(Integer max) {
    return integer(Integer{}, max);
}

// Returns a random real number in the range [min, max).
template<typename Real = double, detail::enable_real_t<Real> = 0>
inline Real real(Real min = Real{0}, Real max = Real{1}) {
    if (max < min) std::swap(min, max);

    std::uniform_real_distribution<Real> distribution(min, max);
    return distribution(engine());
}

// Returns a random arithmetic value. Integers use [min, max], reals use
// [min, max).
template<
    typename Min,
    typename Max,
    typename Result = std::common_type_t<Min, Max>,
    std::enable_if_t<
        detail::is_number_v<Min> &&
        detail::is_number_v<Max>,
        int> = 0>
inline Result number(Min min, Max max) {
    if constexpr (std::is_integral_v<Result>) {
        return integer<Result>(
            static_cast<Result>(min),
            static_cast<Result>(max));
    } else {
        return real<Result>(
            static_cast<Result>(min),
            static_cast<Result>(max));
    }
}

// Returns a random arithmetic value between 0 and max.
template<typename Number, detail::enable_number_t<Number> = 0>
inline Number number(Number max) {
    return number(Number{}, max);
}

// Returns true with the selected probability. Values are clamped to [0, 1].
inline bool boolean(double true_probability = 0.5) {
    std::bernoulli_distribution distribution(
        detail::normalized_probability(true_probability));
    return distribution(engine());
}

// Alias for boolean(probability), useful in conditions.
inline bool chance(double probability) {
    return boolean(probability);
}

// Returns a valid random index for a sequence of size count.
// For count == 0, returns 0.
inline std::size_t index(std::size_t count) {
    if (count == 0) return 0;
    return integer<std::size_t>(0, count - 1);
}

// Returns a pointer to a random element, or nullptr when the container is
// empty.
template<typename Container>
inline auto choice(Container& values)
    -> decltype(std::addressof(*std::begin(values))) {
    auto first = std::begin(values);
    auto last = std::end(values);
    const auto count = std::distance(first, last);
    if (count <= 0) return nullptr;

    std::advance(
        first,
        static_cast<typename std::iterator_traits<decltype(first)>::difference_type>(
            index(static_cast<std::size_t>(count))));
    return std::addressof(*first);
}

// Returns a pointer to a random const element, or nullptr when the container is
// empty.
template<typename Container>
inline auto choice(const Container& values)
    -> decltype(std::addressof(*std::begin(values))) {
    auto first = std::begin(values);
    auto last = std::end(values);
    const auto count = std::distance(first, last);
    if (count <= 0) return nullptr;

    std::advance(
        first,
        static_cast<typename std::iterator_traits<decltype(first)>::difference_type>(
            index(static_cast<std::size_t>(count))));
    return std::addressof(*first);
}

// Shuffles a random-access iterator range.
template<typename Iterator>
inline void shuffle(Iterator first, Iterator last) {
    std::shuffle(first, last, engine());
}

// Shuffles a random-access container in place.
template<typename Container>
inline void shuffle(Container& values) {
    shuffle(std::begin(values), std::end(values));
}

// Fills a byte buffer with pseudo-random bytes.
inline void bytes(unsigned char* output, std::size_t count) {
    if (output == nullptr) return;

    std::uniform_int_distribution<int> distribution(0, 255);
    for (std::size_t index = 0; index < count; ++index) {
        output[index] = static_cast<unsigned char>(distribution(engine()));
    }
}

// Returns count pseudo-random bytes.
inline std::vector<unsigned char> bytes(std::size_t count) {
    std::vector<unsigned char> result(count);
    bytes(result.data(), result.size());
    return result;
}

// Returns a random string built from alphabet.
inline std::string string(
    std::size_t length,
    std::string_view alphabet = detail::default_alphabet()) {
    std::string result;
    if (alphabet.empty()) return result;

    result.reserve(length);
    for (std::size_t current = 0; current < length; ++current) {
        result.push_back(alphabet[index(alphabet.size())]);
    }

    return result;
}

// Returns a random lowercase hexadecimal string.
inline std::string hex(std::size_t length) {
    return string(length, detail::hex_alphabet());
}

} // namespace ign::random
