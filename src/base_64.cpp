#include "private/base_64.hpp"

const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const u8 decode_table[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
    45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64};

std::string encode_base64(const std::vector<u8> &data) {
    size_t in_len = data.size();
    size_t out_len = 4 * ((in_len + 2) / 3);
    std::string result(out_len, '\0');

    size_t i = 0;
    size_t j = 0;

    // Process input data 3 bytes at a time
    for (; i + 2 < in_len; i += 3) {
        result[j++] = base64_chars[(data[i] >> 2) & 0x3F];
        result[j++] = base64_chars[((data[i] & 0x3) << 4) | ((data[i + 1] & 0xF0) >> 4)];
        result[j++] = base64_chars[((data[i + 1] & 0xF) << 2) | ((data[i + 2] & 0xC0) >> 6)];
        result[j++] = base64_chars[data[i + 2] & 0x3F];
    }

    // Handle remaining bytes
    if (i < in_len) {
        result[j++] = base64_chars[(data[i] >> 2) & 0x3F];

        if (i + 1 < in_len) {
            result[j++] = base64_chars[((data[i] & 0x3) << 4) | ((data[i + 1] & 0xF0) >> 4)];
            result[j++] = base64_chars[((data[i + 1] & 0xF) << 2)];
        } else {
            result[j++] = base64_chars[((data[i] & 0x3) << 4)];
            result[j++] = '=';
        }

        result[j++] = '=';
    }

    return result;
}

std::vector<u8> decode_base_64(const std::string &input) {
    // Skip any padding characters ('=')
    usize input_length = input.size();
    while (input_length > 0 && input[input_length - 1] == '=') {
        input_length--;
    }

    // Calculate output size (will be at most 3/4 of input size)
    usize output_length = input_length * 3 / 4;
    std::vector<u8> output;
    output.reserve(output_length);

    u8 val[4];
    usize val_count = 0;

    // Process each input character
    for (usize i = 0; i < input_length; i++) {
        u8 c = input[i];

        // Skip whitespace and other non-base64 chars
        if (decode_table[c] == 64) {
            continue;
        }

        val[val_count++] = decode_table[c];

        if (val_count == 4) {
            // We have 4 characters, which gives us 3 bytes
            output.push_back((val[0] << 2) | (val[1] >> 4));
            output.push_back((val[1] << 4) | (val[2] >> 2));
            output.push_back((val[2] << 6) | val[3]);
            val_count = 0;
        }
    }

    // Handle remaining characters
    if (val_count > 1) {
        output.push_back((val[0] << 2) | (val[1] >> 4));
    }
    if (val_count > 2) {
        output.push_back((val[1] << 4) | (val[2] >> 2));
    }

    return output;
}
