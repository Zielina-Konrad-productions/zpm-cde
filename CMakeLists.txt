cmake_minimum_required(VERSION 3.20)
project(zpm-cde VERSION 0.1.0 LANGUAGES CXX)

# Standard C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. NAJPIERW tworzymy cel wykonywalny z podanych plików .cpp
add_executable(zpm-cde
    src/zpm-cde.cpp
    src/commands/install.cpp
    src/commands/remove.cpp
    src/commands/uninstall.cpp
    src/commands/update.cpp
    src/commands/upgrade.cpp
)

add_executable(INST

    src/INSTALL.cpp
)

# 2. DOPIERO TERAZ dodajemy katalogi nagłówkowe dla utworzonego wyżej zpm
target_include_directories(zpm-cde PRIVATE
    src/commands/declaration
    zpm-lib
    zpm-lib/modules
)

target_compile_options(zpm-cde PRIVATE -Wall -Wextra)
target_link_options(zpm-cde PRIVATE -static)

target_compile_options(INST PRIVATE -Wall -Wextra)
target_link_options(INST PRIVATE -static)
