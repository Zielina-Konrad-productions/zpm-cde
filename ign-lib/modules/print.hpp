#pragma once

#include <iostream>
#include <utility>

#include "color.hpp"

namespace ign::print_detail {

inline constexpr const char* CLEAR_LINE_END = "\033[K";

template<typename... Args>
inline void write(std::ostream& stream, bool newline, Args&&... args) {
    (stream << ... << std::forward<Args>(args));

    if (newline) {
        stream.put('\n');
    }

    stream.flush();
}

template<typename... Args>
inline void write_styled(
    std::ostream& stream,
    const char* style,
    bool newline,
    Args&&... args
) {
    stream << style;
    (stream << ... << std::forward<Args>(args));
    stream << color::RESET;

    if (newline) {
        stream.put('\n');
    }

    stream.flush();
}

template<typename... Args>
inline void write_each(std::ostream& stream, Args&&... args) {
    ((stream << std::forward<Args>(args) << '\n'), ...);
    stream.flush();
}

template<typename... Args>
inline void write_each_styled(
    std::ostream& stream,
    const char* style,
    Args&&... args
) {
    ((stream << style << std::forward<Args>(args) << color::RESET << '\n'), ...);
    stream.flush();
}

template<typename... Args>
inline void rewrite_line(std::ostream& stream, bool newline, Args&&... args) {
    stream << '\r' << CLEAR_LINE_END;
    (stream << ... << std::forward<Args>(args));

    if (newline) {
        stream.put('\n');
    }

    stream.flush();
}

template<typename... Args>
inline void rewrite_line_styled(
    std::ostream& stream,
    const char* style,
    bool newline,
    Args&&... args
) {
    stream << '\r' << CLEAR_LINE_END << style;
    (stream << ... << std::forward<Args>(args));
    stream << color::RESET;

    if (newline) {
        stream.put('\n');
    }

    stream.flush();
}

} // namespace ign::print_detail

namespace ign {

// All print functions accept one or more streamable values:
//   ign::println_green("Exit code: ", code);
template<typename... Args>
inline void print(Args&&... args) {
    print_detail::write(
        std::cout,
        false,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void println(Args&&... args) {
    print_detail::write(
        std::cout,
        true,
        std::forward<Args>(args)...
    );
}

// Prints every argument on a separate line.
// Usage example: ign::println_each("first", "second", "third");
template<typename... Args>
inline void println_each(Args&&... args) {
    print_detail::write_each(
        std::cout,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void print_rewriteln(Args&&... args) {
    print_detail::rewrite_line(
        std::cout,
        false,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void println_rewriteln(Args&&... args) {
    print_detail::rewrite_line(
        std::cout,
        true,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void print_error(Args&&... args) {
    print_detail::write(
        std::cerr,
        false,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void println_error(Args&&... args) {
    print_detail::write(
        std::cerr,
        true,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void println_each_error(Args&&... args) {
    print_detail::write_each(
        std::cerr,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void print_error_rewriteln(Args&&... args) {
    print_detail::rewrite_line(
        std::cerr,
        false,
        std::forward<Args>(args)...
    );
}

template<typename... Args>
inline void println_error_rewriteln(Args&&... args) {
    print_detail::rewrite_line(
        std::cerr,
        true,
        std::forward<Args>(args)...
    );
}

// These wrappers keep the existing print_NAME()/println_NAME() API while the
// implementation remains in one place.
#define IGN_CREATE_PRINT_STYLE(NAME, CODE)                              \
    template<typename... Args>                                         \
    inline void print_##NAME(Args&&... args) {                          \
        print_detail::write_styled(                                    \
            std::cout,                                                 \
            color::CODE,                                               \
            false,                                                     \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void println_##NAME(Args&&... args) {                        \
        print_detail::write_styled(                                    \
            std::cout,                                                 \
            color::CODE,                                               \
            true,                                                      \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void println_each_##NAME(Args&&... args) {                   \
        print_detail::write_each_styled(                               \
            std::cout,                                                 \
            color::CODE,                                               \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void print_rewriteln_##NAME(Args&&... args) {                \
        print_detail::rewrite_line_styled(                             \
            std::cout,                                                 \
            color::CODE,                                               \
            false,                                                     \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void println_rewriteln_##NAME(Args&&... args) {              \
        print_detail::rewrite_line_styled(                             \
            std::cout,                                                 \
            color::CODE,                                               \
            true,                                                      \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void print_error_##NAME(Args&&... args) {                    \
        print_detail::write_styled(                                    \
            std::cerr,                                                 \
            color::CODE,                                               \
            false,                                                     \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void println_error_##NAME(Args&&... args) {                  \
        print_detail::write_styled(                                    \
            std::cerr,                                                 \
            color::CODE,                                               \
            true,                                                      \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void println_each_error_##NAME(Args&&... args) {             \
        print_detail::write_each_styled(                               \
            std::cerr,                                                 \
            color::CODE,                                               \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void print_error_rewriteln_##NAME(Args&&... args) {          \
        print_detail::rewrite_line_styled(                             \
            std::cerr,                                                 \
            color::CODE,                                               \
            false,                                                     \
            std::forward<Args>(args)...                                \
        );                                                             \
    }                                                                  \
                                                                       \
    template<typename... Args>                                         \
    inline void println_error_rewriteln_##NAME(Args&&... args) {        \
        print_detail::rewrite_line_styled(                             \
            std::cerr,                                                 \
            color::CODE,                                               \
            true,                                                      \
            std::forward<Args>(args)...                                \
        );                                                             \
    }

// Generate all named print functions from the list in color.hpp. ANSI_CODE is
// deliberately unused here.
#define IGN_DETAIL_CREATE_PRINT_STYLE(NAME, CONSTANT, ANSI_CODE) \
    IGN_CREATE_PRINT_STYLE(NAME, CONSTANT)

IGN_DETAIL_FOR_EACH_COLOR_STYLE(IGN_DETAIL_CREATE_PRINT_STYLE)

#undef IGN_DETAIL_CREATE_PRINT_STYLE

#undef IGN_CREATE_PRINT_STYLE

} // namespace ign
