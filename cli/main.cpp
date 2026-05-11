#include <leonmusic/api.hpp>

#include <iostream>

#include "../src/internal.hpp"

int main(int argc, char** argv) {
    leonmusic::LeonMusicApi api {};
    const leonmusic::Result result = leonmusic::runCliCommand(api, argc, argv);
    if (!result.ok()) {
        std::cerr << result.message << std::endl;
        return static_cast<int>(result.code);
    }
    return 0;
}

