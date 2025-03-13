#pragma once

#include "common/types.hpp"
#include <string>
#include <vector>

std::string encode_base64(const std::vector<u8> &data);
std::vector<u8> decode_base_64(const std::string &input);
