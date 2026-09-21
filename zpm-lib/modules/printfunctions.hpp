#pragma once

#include <iostream>

namespace zpm {

    template <typename... Args>
    inline void out(const Args&... args) {

        std::ios_base::sync_with_stdio(false);
        (std::cout << ... << args); 
    }


    template <typename... Args>
    inline void outl(const Args&... args) {
        
        std::ios_base::sync_with_stdio(false);
        (std::cout << ... << args); 
        std::cout << '\n'; 
    }
}

