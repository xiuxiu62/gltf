#include "private/fs.hpp"
#include "public/gltf.hpp"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>

std::string create_json_string(const std::string &str) {
    std::ostringstream oss;
    oss << "\"";

    for (char c : str) {
        if (c == '\"') {
            oss << "\\\"";
        } else if (c == '\\') {
            oss << "\\\\";
        } else if (c == '\b') {
            oss << "\\b";
        } else if (c == '\f') {
            oss << "\\f";
        } else if (c == '\n') {
            oss << "\\n";
        } else if (c == '\r') {
            oss << "\\r";
        } else if (c == '\t') {
            oss << "\\t";
        } else if (static_cast<unsigned char>(c) < 32) {
            oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<i32>(c);
        } else {
            oss << c;
        }
    }

    oss << "\"";
    return oss.str();
}

namespace gltf {

std::string Model::generate_json(bool for_glb) {
    std::ostringstream json;
    json << "{";

    // Asset section (required)
    json << "\"asset\":{\"version\":\"2.0\"}";

    // Scene section
    if (!scenes.empty()) {
        json << ",\"scene\":" << default_scene;

        // Scenes array
        json << ",\"scenes\":[";
        for (usize i = 0; i < scenes.size(); i++) {
            if (i > 0) json << ",";

            json << "{";

            // Scene nodes
            if (!scenes[i].nodes.empty()) {
                json << "\"nodes\":[";
                for (usize j = 0; j < scenes[i].nodes.size(); j++) {
                    if (j > 0) json << ",";
                    json << scenes[i].nodes[j];
                }
                json << "]";
            }

            // Scene name
            if (!scenes[i].name.empty()) {
                json << ",\"name\":" << create_json_string(scenes[i].name);
            }

            json << "}";
        }
        json << "]";
    }

    // Nodes array
    if (!nodes.empty()) {
        json << ",\"nodes\":[";
        for (usize i = 0; i < nodes.size(); i++) {
            if (i > 0) json << ",";

            json << "{";

            // Children
            if (!nodes[i].children.empty()) {
                json << "\"children\":[";
                for (usize j = 0; j < nodes[i].children.size(); j++) {
                    if (j > 0) json << ",";
                    json << nodes[i].children[j];
                }
                json << "]";
            }

            // Mesh reference
            if (nodes[i].mesh != UINT32_MAX) {
                if (!nodes[i].children.empty()) json << ",";
                json << "\"mesh\":" << nodes[i].mesh;
            }

            // Skin reference
            if (nodes[i].skin != UINT32_MAX) {
                if (!nodes[i].children.empty() || nodes[i].mesh != UINT32_MAX) json << ",";
                json << "\"skin\":" << nodes[i].skin;
            }

            // Transformation (matrix OR translation/rotation/scale)
            bool has_transform_prop =
                !nodes[i].children.empty() || nodes[i].mesh != UINT32_MAX || nodes[i].skin != UINT32_MAX;

            if (nodes[i].has_matrix) {
                if (has_transform_prop) json << ",";

                json << "\"matrix\":[";
                for (usize j = 0; j < 16; j++) {
                    if (j > 0) json << ",";
                    json << nodes[i].matrix.m[j];
                }
                json << "]";
            } else {
                bool has_transform = false;

                // Translation
                if (nodes[i].translation.x != 0.0f || nodes[i].translation.y != 0.0f ||
                    nodes[i].translation.z != 0.0f) {
                    if (has_transform_prop) json << ",";
                    has_transform = true;

                    json << "\"translation\":[" << nodes[i].translation.x << "," << nodes[i].translation.y << ","
                         << nodes[i].translation.z << "]";
                }

                // Rotation
                if (nodes[i].rotation.x != 0.0f || nodes[i].rotation.y != 0.0f || nodes[i].rotation.z != 0.0f ||
                    nodes[i].rotation.w != 1.0f) {
                    if (has_transform_prop || has_transform) json << ",";
                    has_transform = true;

                    json << "\"rotation\":[" << nodes[i].rotation.x << "," << nodes[i].rotation.y << ","
                         << nodes[i].rotation.z << "," << nodes[i].rotation.w << "]";
                }

                // Scale
                if (nodes[i].scale.x != 1.0f || nodes[i].scale.y != 1.0f || nodes[i].scale.z != 1.0f) {
                    if (has_transform_prop || has_transform) json << ",";

                    json << "\"scale\":[" << nodes[i].scale.x << "," << nodes[i].scale.y << "," << nodes[i].scale.z
                         << "]";
                }
            }

            // Name
            if (!nodes[i].name.empty()) {
                if (has_transform_prop || nodes[i].has_matrix ||
                    (nodes[i].translation.x != 0.0f || nodes[i].translation.y != 0.0f ||
                     nodes[i].translation.z != 0.0f) ||
                    (nodes[i].rotation.x != 0.0f || nodes[i].rotation.y != 0.0f || nodes[i].rotation.z != 0.0f ||
                     nodes[i].rotation.w != 1.0f) ||
                    (nodes[i].scale.x != 1.0f || nodes[i].scale.y != 1.0f || nodes[i].scale.z != 1.0f)) {
                    json << ",";
                }

                json << "\"name\":" << create_json_string(nodes[i].name);
            }

            json << "}";
        }
        json << "]";
    }

    // Meshes array
    if (!meshes.empty()) {
        json << ",\"meshes\":[";
        for (usize i = 0; i < meshes.size(); i++) {
            if (i > 0) json << ",";

            json << "{";

            // Primitives array (required)
            json << "\"primitives\":[";
            for (usize j = 0; j < meshes[i].primitives.size(); j++) {
                if (j > 0) json << ",";

                json << "{";

                // Attributes (required)
                json << "\"attributes\":{";
                bool first_attr = true;
                for (const auto &attr : meshes[i].primitives[j].attributes) {
                    if (!first_attr) json << ",";
                    first_attr = false;

                    json << "\"" << attr.first << "\":" << attr.second;
                }
                json << "}";

                // Indices
                if (meshes[i].primitives[j].indices != UINT32_MAX) {
                    json << ",\"indices\":" << meshes[i].primitives[j].indices;
                }

                // Material
                if (meshes[i].primitives[j].material != UINT32_MAX) {
                    json << ",\"material\":" << meshes[i].primitives[j].material;
                }

                // Mode
                if (meshes[i].primitives[j].mode != 4) {
                    json << ",\"mode\":" << meshes[i].primitives[j].mode;
                }

                json << "}";
            }
            json << "]";

            // Weights
            if (!meshes[i].weights.empty()) {
                json << ",\"weights\":[";
                for (usize j = 0; j < meshes[i].weights.size(); j++) {
                    if (j > 0) json << ",";
                    json << meshes[i].weights[j];
                }
                json << "]";
            }

            // Name
            if (!meshes[i].name.empty()) {
                json << ",\"name\":" << create_json_string(meshes[i].name);
            }

            json << "}";
        }
        json << "]";
    }

    // Materials array
    if (!materials.empty()) {
        json << ",\"materials\":[";
        for (usize i = 0; i < materials.size(); i++) {
            if (i > 0) json << ",";

            json << "{";

            // PBR Metallic Roughness
            json << "\"pbrMetallicRoughness\":{";

            // Base color factor
            if (materials[i].pbr_metallic_roughness.base_color_factor.x != 1.0f ||
                materials[i].pbr_metallic_roughness.base_color_factor.y != 1.0f ||
                materials[i].pbr_metallic_roughness.base_color_factor.z != 1.0f ||
                materials[i].pbr_metallic_roughness.base_color_factor.w != 1.0f) {
                json << "\"baseColorFactor\":[" << materials[i].pbr_metallic_roughness.base_color_factor.x << ","
                     << materials[i].pbr_metallic_roughness.base_color_factor.y << ","
                     << materials[i].pbr_metallic_roughness.base_color_factor.z << ","
                     << materials[i].pbr_metallic_roughness.base_color_factor.w << "]";
            }

            // Base color texture
            if (materials[i].pbr_metallic_roughness.base_color_texture.index != 0) {
                if (materials[i].pbr_metallic_roughness.base_color_factor.x != 1.0f ||
                    materials[i].pbr_metallic_roughness.base_color_factor.y != 1.0f ||
                    materials[i].pbr_metallic_roughness.base_color_factor.z != 1.0f ||
                    materials[i].pbr_metallic_roughness.base_color_factor.w != 1.0f) {
                    json << ",";
                }

                json << "\"baseColorTexture\":{\"index\":"
                     << materials[i].pbr_metallic_roughness.base_color_texture.index;

                if (materials[i].pbr_metallic_roughness.base_color_texture.tex_coord != 0) {
                    json << ",\"texCoord\":" << materials[i].pbr_metallic_roughness.base_color_texture.tex_coord;
                }

                json << "}";
            }

            // Metallic factor
            if (materials[i].pbr_metallic_roughness.metallic_factor != 1.0f) {
                if ((materials[i].pbr_metallic_roughness.base_color_factor.x != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.y != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.z != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.w != 1.0f) ||
                    materials[i].pbr_metallic_roughness.base_color_texture.index != 0) {
                    json << ",";
                }

                json << "\"metallicFactor\":" << materials[i].pbr_metallic_roughness.metallic_factor;
            }

            // Roughness factor
            if (materials[i].pbr_metallic_roughness.roughness_factor != 1.0f) {
                if ((materials[i].pbr_metallic_roughness.base_color_factor.x != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.y != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.z != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.w != 1.0f) ||
                    materials[i].pbr_metallic_roughness.base_color_texture.index != 0 ||
                    materials[i].pbr_metallic_roughness.metallic_factor != 1.0f) {
                    json << ",";
                }

                json << "\"roughnessFactor\":" << materials[i].pbr_metallic_roughness.roughness_factor;
            }

            // Metallic roughness texture
            if (materials[i].pbr_metallic_roughness.metallic_roughness_texture.index != 0) {
                if ((materials[i].pbr_metallic_roughness.base_color_factor.x != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.y != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.z != 1.0f ||
                     materials[i].pbr_metallic_roughness.base_color_factor.w != 1.0f) ||
                    materials[i].pbr_metallic_roughness.base_color_texture.index != 0 ||
                    materials[i].pbr_metallic_roughness.metallic_factor != 1.0f ||
                    materials[i].pbr_metallic_roughness.roughness_factor != 1.0f) {
                    json << ",";
                }

                json << "\"metallicRoughnessTexture\":{\"index\":"
                     << materials[i].pbr_metallic_roughness.metallic_roughness_texture.index;

                if (materials[i].pbr_metallic_roughness.metallic_roughness_texture.tex_coord != 0) {
                    json << ",\"texCoord\":"
                         << materials[i].pbr_metallic_roughness.metallic_roughness_texture.tex_coord;
                }

                json << "}";
            }

            json << "}";

            // Normal texture
            if (materials[i].normal_texture.index != 0) {
                json << ",\"normalTexture\":{\"index\":" << materials[i].normal_texture.index;

                if (materials[i].normal_texture.tex_coord != 0) {
                    json << ",\"texCoord\":" << materials[i].normal_texture.tex_coord;
                }

                json << "}";
            }

            // Occlusion texture
            if (materials[i].occlusion_texture.index != 0) {
                json << ",\"occlusionTexture\":{\"index\":" << materials[i].occlusion_texture.index;

                if (materials[i].occlusion_texture.tex_coord != 0) {
                    json << ",\"texCoord\":" << materials[i].occlusion_texture.tex_coord;
                }

                json << "}";
            }

            // Emissive texture
            if (materials[i].emissive_texture.index != 0) {
                json << ",\"emissiveTexture\":{\"index\":" << materials[i].emissive_texture.index;

                if (materials[i].emissive_texture.tex_coord != 0) {
                    json << ",\"texCoord\":" << materials[i].emissive_texture.tex_coord;
                }

                json << "}";
            }

            // Emissive factor
            if (materials[i].emissive_factor.x != 0.0f || materials[i].emissive_factor.y != 0.0f ||
                materials[i].emissive_factor.z != 0.0f) {
                json << ",\"emissiveFactor\":[" << materials[i].emissive_factor.x << ","
                     << materials[i].emissive_factor.y << "," << materials[i].emissive_factor.z << "]";
            }

            // Alpha mode
            if (materials[i].alpha_mode != "OPAQUE") {
                json << ",\"alphaMode\":" << create_json_string(materials[i].alpha_mode);
            }

            // Alpha cutoff
            if (materials[i].alpha_cutoff != 0.5f && materials[i].alpha_mode == "MASK") {
                json << ",\"alphaCutoff\":" << materials[i].alpha_cutoff;
            }

            // Double sided
            if (materials[i].double_sided) {
                json << ",\"doubleSided\":true";
            }

            // Name
            if (!materials[i].name.empty()) {
                json << ",\"name\":" << create_json_string(materials[i].name);
            }

            json << "}";
        }
        json << "]";
    }

    // Accessors array
    if (!accessors.empty()) {
        json << ",\"accessors\":[";
        for (usize i = 0; i < accessors.size(); i++) {
            if (i > 0) json << ",";

            json << "{";

            // BufferView
            json << "\"bufferView\":" << accessors[i].buffer_view;

            // ByteOffset
            if (accessors[i].byte_offset != 0) {
                json << ",\"byteOffset\":" << accessors[i].byte_offset;
            }

            // ComponentType (required)
            json << ",\"componentType\":" << accessors[i].component_type;

            // Normalized
            if (accessors[i].normalized) {
                json << ",\"normalized\":true";
            }

            // Count (required)
            json << ",\"count\":" << accessors[i].count;

            // Type (required)
            json << ",\"type\":" << create_json_string(accessors[i].type);

            // Min values
            if (!accessors[i].min.empty()) {
                json << ",\"min\":[";
                for (usize j = 0; j < accessors[i].min.size(); j++) {
                    if (j > 0) json << ",";
                    json << accessors[i].min[j];
                }
                json << "]";
            }

            // Max values
            if (!accessors[i].max.empty()) {
                json << ",\"max\":[";
                for (usize j = 0; j < accessors[i].max.size(); j++) {
                    if (j > 0) json << ",";
                    json << accessors[i].max[j];
                }
                json << "]";
            }

            // Name
            if (!accessors[i].name.empty()) {
                json << ",\"name\":" << create_json_string(accessors[i].name);
            }

            json << "}";
        }
        json << "]";
    }

    // BufferViews array
    if (!buffer_views.empty()) {
        json << ",\"bufferViews\":[";
        for (usize i = 0; i < buffer_views.size(); i++) {
            if (i > 0) json << ",";

            json << "{";

            // Buffer (required)
            json << "\"buffer\":" << buffer_views[i].buffer;

            // ByteOffset
            if (buffer_views[i].byte_offset != 0) {
                json << ",\"byteOffset\":" << buffer_views[i].byte_offset;
            }

            // ByteLength (required)
            json << ",\"byteLength\":" << buffer_views[i].byte_length;

            // ByteStride
            if (buffer_views[i].byte_stride != 0) {
                json << ",\"byteStride\":" << buffer_views[i].byte_stride;
            }

            // Target
            if (buffer_views[i].target != 0) {
                json << ",\"target\":" << buffer_views[i].target;
            }

            // Name
            if (!buffer_views[i].name.empty()) {
                json << ",\"name\":" << create_json_string(buffer_views[i].name);
            }

            json << "}";
        }
        json << "]";
    }

    // Buffers array
    if (!buffers.empty()) {
        json << ",\"buffers\":[";

        if (for_glb) {
            // For GLB, just include the byte length
            json << "{\"byteLength\":" << buffers[0].byte_length << "}";
        } else {
            for (usize i = 0; i < buffers.size(); i++) {
                if (i > 0) json << ",";

                json << "{";
                // ByteLength (required)
                json << "\"byteLength\":" << buffers[i].byte_length;

                // Add URI placeholder - this should be filled by the calling function
                json << ",\"uri\":\"buffer" << i << "\"";

                json << "}";
            }
        }
        json << "]";
    }
    json << "}";

    return json.str();
}

bool Model::save_as_gltf(const std::string &path, bool embed_buffers) {
    std::string json_content = generate_json(false);

    // Write JSON to file
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Failed to create output file: " << path << std::endl;
        return false;
    }

    file << json_content;
    file.close();

    // Extract directory and base name from path
    std::string dir_path;
    usize last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        dir_path = path.substr(0, last_slash + 1);
    }

    std::string base_name;
    usize last_dot = path.find_last_of(".");
    if (last_dot != std::string::npos && (last_slash == std::string::npos || last_dot > last_slash)) {
        base_name = path.substr(last_slash + 1, last_dot - last_slash - 1);
    } else {
        base_name = path.substr(last_slash + 1);
    }

    // Write buffer files
    if (!embed_buffers) {
        for (usize i = 0; i < buffers.size(); i++) {
            std::string buffer_filename = base_name + "_buffer" + std::to_string(i) + ".bin";
            std::string buffer_path = dir_path + buffer_filename;

            std::ofstream buffer_file(buffer_path, std::ios::binary);
            if (!buffer_file) {
                std::cerr << "Failed to create buffer file: " << buffer_path << std::endl;
                return false;
            }

            buffer_file.write(reinterpret_cast<const char *>(buffers[i].data.data()), buffers[i].data.size());
            buffer_file.close();
        }
    }

    return true;
}

bool Model::save_as_glb(const std::string &path) {
    // Use common JSON generation with for_glb=true
    std::string json_content = generate_json(true);

    // Calculate padding needed to align JSON chunk to 4-byte boundary
    usize json_padding = (4 - (json_content.size() % 4)) % 4;
    usize padded_json_length = json_content.size() + json_padding;

    // Combine all buffer data into a single binary chunk
    std::vector<u8> bin_data;
    for (const auto &buffer : buffers) {
        bin_data.insert(bin_data.end(), buffer.data.begin(), buffer.data.end());
    }

    // Calculate padding needed to align BIN chunk to 4-byte boundary
    usize bin_padding = (4 - (bin_data.size() % 4)) % 4;
    usize padded_bin_length = bin_data.size() + bin_padding;

    // Calculate total file size
    // GLB Header (12 bytes) + JSON chunk header (8 bytes) + JSON content + JSON padding +
    // BIN chunk header (8 bytes) + BIN content + BIN padding
    usize total_size = 12 + 8 + padded_json_length + 8 + padded_bin_length;

    // Open output file
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create output file: " << path << std::endl;
        return false;
    }

    // Write GLB header
    // Magic
    const u32 magic = 0x46546C67; // "glTF"
    file.write(reinterpret_cast<const char *>(&magic), 4);

    // Version
    const u32 version = 2;
    file.write(reinterpret_cast<const char *>(&version), 4);

    // Total length
    file.write(reinterpret_cast<const char *>(&total_size), 4);

    // Write JSON chunk
    // JSON chunk length
    u32 json_chunk_length = static_cast<u32>(padded_json_length);
    file.write(reinterpret_cast<const char *>(&json_chunk_length), 4);

    // JSON chunk type
    const u32 json_chunk_type = 0x4E4F534A; // "JSON"
    file.write(reinterpret_cast<const char *>(&json_chunk_type), 4);

    // JSON chunk data
    file.write(json_content.c_str(), json_content.size());

    // JSON padding
    for (usize i = 0; i < json_padding; i++) {
        file.put(' ');
    }

    // Write BIN chunk (only if we have buffer data)
    if (!bin_data.empty()) {
        // BIN chunk length
        u32 bin_chunk_length = static_cast<u32>(padded_bin_length);
        file.write(reinterpret_cast<const char *>(&bin_chunk_length), 4);

        // BIN chunk type
        const u32 bin_chunk_type = 0x004E4942; // "BIN"
        file.write(reinterpret_cast<const char *>(&bin_chunk_type), 4);

        // BIN chunk data
        file.write(reinterpret_cast<const char *>(bin_data.data()), bin_data.size());

        // BIN padding
        for (usize i = 0; i < bin_padding; i++) {
            file.put('\0');
        }
    }

    file.close();

    return true;
}

}; // namespace gltf
