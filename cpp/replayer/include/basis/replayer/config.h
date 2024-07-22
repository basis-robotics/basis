#pragma once

#include <filesystem>

namespace basis::replayer {
    struct Config {
        bool loop = false;
        std::filesystem::path input;
    };
}