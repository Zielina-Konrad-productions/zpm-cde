#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <utility>

namespace ign {

// A string type that reads an entire line, including spaces.
// Pressing Enter without typing anything stores an empty string.
//
// Example:
// ign::string text;
// ign::input(text);
//
// if (text.empty()) {
//     ign::println("empty");
// }

class string : public std::string {
public:
    using std::string::string;
    using std::string::operator=;

    string() = default;

    string(const std::string& value)
        : std::string(value) {}

    string(std::string&& value) noexcept
        : std::string(std::move(value)) {}

    // Allows ign::string to be used wherever the library expects a path.
    operator std::filesystem::path() const {
        return std::filesystem::path(
            static_cast<const std::string&>(*this));
    }

    friend std::istream& operator>>(std::istream& input, string& value) {
        return std::getline(
            input,
            static_cast<std::string&>(value)
        );
    }
};

} // namespace ign
