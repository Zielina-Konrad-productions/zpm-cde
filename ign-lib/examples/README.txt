IGN-LIB EXAMPLES
================

Source files live in:

    examples/src

Built binaries are written to:

    examples/bin


BUILD ALL EXAMPLES
------------------

From the repository root:

    ./examples/build.sh


RUN EXAMPLES
------------

    ./examples/bin/file_status
    ./examples/bin/random_time
    ./examples/bin/run_commands


NOTES
-----

The build script compiles every .cpp file in examples/src with:

    g++ -std=c++17 -Wall -Wextra -I.

The examples write temporary files under /tmp on Unix-like systems.
