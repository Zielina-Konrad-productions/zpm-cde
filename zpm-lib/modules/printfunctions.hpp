#pragma once

#include <iostream>

namespace zpm {

    template <typename... Args>
    inline void out(const Args&... args) {
        
        (std::cout << ... << args); 
    }


    template <typename... Args>
    inline void outl(const Args&... args) {
        
        (std::cout << ... << args); 

        std::cout << '\n'; 
    }
}

