#pragma once

namespace ign {

enum class status {
    ok = 0,
    error = 1,
    not_found = 2,
    permission_denied = 3
};

namespace detail {

inline constexpr int status_severity(status value) noexcept {
    switch (value) {
    case status::ok:
        return 0;
    case status::not_found:
        return 1;
    case status::error:
        return 2;
    case status::permission_denied:
        return 3;
    }

    return 2;
}

inline constexpr status merge_status(status current, status next) noexcept {
    return status_severity(next) > status_severity(current)
        ? next
        : current;
}

} // namespace detail

} // namespace ign
