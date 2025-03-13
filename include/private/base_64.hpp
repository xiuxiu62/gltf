#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace gltf {

std::string encode_base64(const std::vector<u8> &data);
std::vector<u8> decode_base_64(const std::string &input);

}; // namespace gltf
