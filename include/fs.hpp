#pragma once

#include "types.hpp"
#include <string>

namespace gltf {

inline std::string parent_directory(const std::string &path) {
    usize pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? "" : path.substr(0, pos);
}

inline std::string get_filename_stem(const std::string &path) {
    // Get filename with extension
    std::string filename = path;
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        filename = path.substr(pos + 1);
    }

    // Remove extension
    pos = filename.find_last_of(".");
    return (pos == std::string::npos) ? filename : filename.substr(0, pos);
}

inline std::string get_file_extension(const std::string &path) {
    size_t pos = path.find_last_of(".");
    return (pos == std::string::npos) ? "" : path.substr(pos);
}

}; // namespace gltf
