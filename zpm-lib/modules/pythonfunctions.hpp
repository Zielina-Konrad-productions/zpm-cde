#pragma once

namespace zpm::pythonfunctions {

    //any function
    //usage if(zpm::pythonfunctions::any(argument 1, argument 2 ... arguments) == true/false) 
    //only works on BOOL type
    template <typename... Args>
    inline bool any(Args... args) {

        return (... || args);
    }


    //all function
    //usage if(zpm::pythonfunctions::all(argument 1, argument 2 ... arguments) == true/false)
    //only works on BOOL type
    template <typename... Args>
    inline bool all(Args... args) {

    bool all_true = (args && ...);
    bool all_false = (!args && ...);
    
        if (all_true) return true;
        if (all_false) return false;
    
            return false;
    }
}