#include <ign.hpp>

int main() {
    const char* path = "/tmp/ign-lib-example.txt";

    ign::status write_result =
        ign::file::write(path, "hello from ign-lib\n");

    if (write_result != ign::status::ok) {
        ign::println_error_red("write failed");
        return 1;
    }

    std::string text;
    ign::status read_result = ign::file::read_to(path, text);

    if (read_result == ign::status::ok) {
        ign::print(text);
    } else if (read_result == ign::status::not_found) {
        ign::println_error_red("file not found");
        return 1;
    } else {
        ign::println_error_red("read failed");
        return 1;
    }

    ign::file::remove(path);
    return 0;
}
