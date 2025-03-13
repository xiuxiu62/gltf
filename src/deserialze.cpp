#include "json.h"

#include "base_64.hpp"
#include "fs.hpp"
#include "gltf.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace gltf {

std::string Model::get_string(const json_value_s *value) {
    if (!value || value->type != json_type_string) return "";

    const json_string_s *str = (const json_string_s *)value->payload;
    return std::string(str->string, str->string_size);
}

i32 Model::get_int(const json_value_s *value) {
    if (!value) return 0;

    if (value->type == json_type_number) {
        const json_number_s *num = (const json_number_s *)value->payload;
        return std::stoi(std::string(num->number, num->number_size));
    }

    if (value->type == json_type_true) return 1;

    return 0;
}

f32 Model::get_float(const json_value_s *value) {
    if (!value || value->type != json_type_number) return 0.0f;

    const json_number_s *num = (const json_number_s *)value->payload;
    return stof(std::string(num->number, num->number_size));
}

bool Model::get_bool(const json_value_s *value) {
    if (!value) return false;
    return (value->type == json_type_true);
}

json_value_s *find_member(const json_object_s *object, const char *name) {
    if (!object) return nullptr;

    json_object_element_s *element = object->start;
    while (element) {
        if (strncmp(element->name->string, name, element->name->string_size) == 0) {
            return element->value;
        }
        element = element->next;
    }

    return nullptr;
}

Model Model::load(bool &success, const std::string &path) {
    Model model;

    model.base_path = parent_directory(path);

    std::ifstream file(path, std::ios::binary);

    if (!file) {
        std::cerr << "Failed to open glTF file: " << path << std::endl;
        success = false;
        return model;
    }

    bool is_glb = path.length() >= 4 && path.substr(path.length() - 4) == ".glb";
    if (is_glb) { // Read entire file for glb processing
        file.seekg(0, std::ios::end);
        usize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        u8 *file_data = (u8 *)malloc(file_size);
        file.read((char *)file_data, file_size);

        model = load_from_memory(success, file_data, file_size);
        free(file_data);
        return model;
    } else {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_content = buffer.str();

        json_value_s *root = json_parse(json_content.c_str(), json_content.length());
        if (!root) {
            std::cerr << "Failed to parse glTF JSON" << std::endl;
            success = false;
            return model;
        }

        auto success = model.parse(root);
        free(root);
        return model;
    }
}

Model Model::load_from_memory(bool &success, const u8 *data, usize length) {
    Model model;

    // Check if this is a GLB file
    bool is_glb = false;
    if (length > 4 && memcmp(data, "glTF", 4) == 0) {
        is_glb = true;
    }

    if (is_glb) {
        // Parse GLB header (12 bytes)
        if (length < 12) {
            std::cerr << "Invalid GLB: too small" << std::endl;
            success = false;
            return model;
        }

        // Check magic and version
        u32 magic = *reinterpret_cast<const u32 *>(data);
        u32 version = *reinterpret_cast<const u32 *>(data + 4);
        u32 total_length = *reinterpret_cast<const u32 *>(data + 8);

        if (magic != 0x46546C67) { // "glTF" in ASCII
            std::cerr << "Invalid GLB: incorrect magic number" << std::endl;
            success = false;
            return model;
        }

        if (version != 2) {
            std::cerr << "Unsupported GLB version: " << version << std::endl;
            success = false;
            return model;
        }

        if (total_length > length) {
            std::cerr << "Invalid GLB: reported length exceeds data size" << std::endl;
            success = false;
            return model;
        }

        // Parse JSON chunk
        if (length < 20) { // Header (12) + Chunk header (8)
            std::cerr << "Invalid GLB: too small for chunk header" << std::endl;
            success = false;
            return model;
        }

        u32 json_chunk_length = *reinterpret_cast<const u32 *>(data + 12);
        u32 json_chunk_type = *reinterpret_cast<const u32 *>(data + 16);

        if (json_chunk_type != 0x4E4F534A) { // "JSON" in ASCII
            std::cerr << "Invalid GLB: first chunk is not JSON" << std::endl;
            success = false;
            return model;
        }

        // Parse JSON content
        const char *json_data = reinterpret_cast<const char *>(data + 20);
        json_value_s *root = json_parse(json_data, json_chunk_length);
        if (!root) {
            std::cerr << "Failed to parse GLB JSON chunk" << std::endl;
            success = false;
            return model;
        }

        // Check for BIN chunk
        usize bin_chunk_start = 20 + json_chunk_length;
        if (length > bin_chunk_start + 8) {
            u32 bin_chunk_length = *reinterpret_cast<const u32 *>(data + bin_chunk_start);
            u32 bin_chunk_type = *reinterpret_cast<const u32 *>(data + bin_chunk_start + 4);

            if (bin_chunk_type == 0x004E4942) { // "BIN" in ASCII
                if (length >= bin_chunk_start + 8 + bin_chunk_length) {
                    // Add a buffer for the BIN chunk
                    Buffer buffer;
                    buffer.byte_length = bin_chunk_length;
                    buffer.data.resize(bin_chunk_length);
                    std::memcpy(buffer.data.data(), data + bin_chunk_start + 8, bin_chunk_length);
                    buffer.loaded = true;
                    model.buffers.push_back(buffer);
                }
            }
        }

        success = model.parse(root);

        // Free the JSON data
        free(root);
        return model;
    } else {
        // Regular JSON glTF
        std::string json_content(reinterpret_cast<const char *>(data), length);

        json_value_s *root = json_parse(json_content.c_str(), json_content.length());
        if (!root) {
            std::cerr << "Failed to parse glTF JSON" << std::endl;
            success = false;
            return model;
        }

        success = model.parse(root);

        // Free the JSON data
        free(root);
        return model;
    }
}

// void Model::destroy() {
//     buffers.destroy();
//     buffer_views.destroy();
//     accessors.destroy();
//     images.destroy();
//     samplers.destroy();
//     textures.destroy();
//     materials.destroy();
//     meshes.destroy();
//     skins.destroy();
//     nodes.destroy();
//     scenes.destroy();
//     animations.destroy();
// }

bool Model::parse(const json_value_s *root) {
    buffers.clear();
    buffer_views.clear();
    accessors.clear();
    images.clear();
    samplers.clear();
    textures.clear();
    materials.clear();
    meshes.clear();
    skins.clear();
    nodes.clear();
    scenes.clear();
    animations.clear();

    if (root->type != json_type_object) {
        std::cerr << "Invalid glTF: root is not an object" << std::endl;
        return false;
    }

    const json_object_s *root_obj = (const json_object_s *)root->payload;

    // Parse asset info (version check)
    json_value_s *asset_value = find_member(root_obj, "asset");
    if (asset_value && asset_value->type == json_type_object) {
        const json_object_s *asset_obj = (const json_object_s *)asset_value->payload;
        json_value_s *version_value = find_member(asset_obj, "version");

        if (version_value) {
            std::string version = get_string(version_value);
            if (version != "2.0") {
                std::cerr << "Warning: glTF version " << version << " may not be fully supported" << std::endl;
            }
        }
    }

    json_value_s *scene_value = find_member(root_obj, "scene");
    if (scene_value) {
        default_scene = get_int(scene_value);
    }

    parse_buffers(root_obj);
    parse_buffer_views(root_obj);
    parse_accessors(root_obj);
    parse_images(root_obj);
    parse_samplers(root_obj);
    parse_textures(root_obj);
    parse_materials(root_obj);
    parse_meshes(root_obj);
    parse_skins(root_obj);
    parse_nodes(root_obj);
    parse_scenes(root_obj);
    parse_animations(root_obj);

    load_buffers();

    return true;
}

bool Model::parse_buffers(const json_object_s *json) {
    json_value_s *buffers_value = find_member(json, "buffers");
    if (!buffers_value || buffers_value->type != json_type_array) return true; // No buffers, that's fine

    const json_array_s *buffers_array = (const json_array_s *)buffers_value->payload;
    json_array_element_s *element = buffers_array->start;

    // Skip if we already loaded a buffer from GLB
    if (!buffers.empty()) {
        return true;
    }

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *buffer_obj = (const json_object_s *)element->value->payload;

            Buffer buffer;

            json_value_s *byte_length_value = find_member(buffer_obj, "byteLength");
            if (byte_length_value) {
                buffer.byte_length = get_int(byte_length_value);
            }

            json_value_s *uri_value = find_member(buffer_obj, "uri");
            if (uri_value) {
                buffer.uri = get_string(uri_value);
            }

            // json_value_s *nameValue = find_member(buffer_obj, "name");
            // if (nameValue) {
            // buffer.name = GetString(nameValue);
            // }

            buffers.push_back(buffer);
        }

        element = element->next;
    }

    return true;
}

// bool Model::parse_buffers(const json_object_s *json) {
//     json_value_s *buffers_value = find_member(json, "buffers");
//     if (!buffers_value || buffers_value->type != json_type_array) return true; // No buffers, that's fine

//     const json_array_s *buffers_array = (const json_array_s *)buffers_value->payload;
//     // buffers = FixedArray<Buffer>::create(buffers_array->length);

//     for (usize i = 0; i < buffers_array->length; i++) {
//         json_array_element_s &element = buffers_array->start[i];

//         if (element.value->type == json_type_object) {
//             const json_object_s *bufferObj = (const json_object_s *)element.value->payload;

//             Buffer buffer;

//             json_value_s *byte_length_value = find_member(bufferObj, "byteLength");
//             if (byte_length_value) {
//                 buffer.byte_length = get_int(byte_length_value);
//             }

//             json_value_s *uri_value = find_member(bufferObj, "uri");
//             if (uri_value) {
//                 buffer.uri = get_string(uri_value);
//             }

//             json_value_s *name_value = find_member(bufferObj, "name");
//             if (name_value) {
//                 // buffer.name = get_string(name_value);
//             }

//             *buffers[i] = buffer;
//         }
//     }

//     return true;
// }

bool Model::parse_buffer_views(const json_object_s *json) {
    json_value_s *buffer_views_value = find_member(json, "bufferViews");
    if (!buffer_views_value || buffer_views_value->type != json_type_array) return true; // No buffer views, that's fine

    const json_array_s *buffer_views_array = (const json_array_s *)buffer_views_value->payload;
    json_array_element_s *element = buffer_views_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *buffer_view_obj = (const json_object_s *)element->value->payload;

            BufferView buffer_view;

            json_value_s *buffer_value = find_member(buffer_view_obj, "buffer");
            if (buffer_value) {
                buffer_view.buffer = get_int(buffer_value);
            }

            json_value_s *byte_offset_value = find_member(buffer_view_obj, "byteOffset");
            if (byte_offset_value) {
                buffer_view.byte_offset = get_int(byte_offset_value);
            }

            json_value_s *byte_length_value = find_member(buffer_view_obj, "byteLength");
            if (byte_length_value) {
                buffer_view.byte_length = get_int(byte_length_value);
            }

            json_value_s *byte_stride_value = find_member(buffer_view_obj, "byteStride");
            if (byte_stride_value) {
                buffer_view.byte_stride = get_int(byte_stride_value);
            }

            json_value_s *target_value = find_member(buffer_view_obj, "target");
            if (target_value) {
                buffer_view.target = get_int(target_value);
            }

            json_value_s *name_value = find_member(buffer_view_obj, "name");
            if (name_value) {
                buffer_view.name = get_string(name_value);
            }

            buffer_views.push_back(buffer_view);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_accessors(const json_object_s *json) {
    json_value_s *accessorsValue = find_member(json, "accessors");
    if (!accessorsValue || accessorsValue->type != json_type_array) return true; // No accessors, that's fine

    const json_array_s *accessorsArray = (const json_array_s *)accessorsValue->payload;
    json_array_element_s *element = accessorsArray->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *accessor_obj = (const json_object_s *)element->value->payload;

            Accessor accessor;

            json_value_s *buffer_view_value = find_member(accessor_obj, "bufferView");
            if (buffer_view_value) {
                accessor.buffer_view = get_int(buffer_view_value);
            }

            json_value_s *byte_offset_value = find_member(accessor_obj, "byteOffset");
            if (byte_offset_value) {
                accessor.byte_offset = get_int(byte_offset_value);
            }

            json_value_s *component_type_value = find_member(accessor_obj, "componentType");
            if (component_type_value) {
                accessor.component_type = get_int(component_type_value);
            }

            json_value_s *normalized_value = find_member(accessor_obj, "normalized");
            if (normalized_value) {
                accessor.normalized = get_bool(normalized_value);
            }

            json_value_s *count_value = find_member(accessor_obj, "count");
            if (count_value) {
                accessor.count = get_int(count_value);
            }

            json_value_s *type_value = find_member(accessor_obj, "type");
            if (type_value) {
                accessor.type = get_string(type_value);
            }

            // Parse min array
            json_value_s *min_value = find_member(accessor_obj, "min");
            if (min_value && min_value->type == json_type_array) {
                const json_array_s *min_array = (const json_array_s *)min_value->payload;
                json_array_element_s *min_element = min_array->start;

                while (min_element) {
                    accessor.min.push_back(get_float(min_element->value));
                    min_element = min_element->next;
                }
            }

            // Parse max array
            json_value_s *max_value = find_member(accessor_obj, "max");
            if (max_value && max_value->type == json_type_array) {
                const json_array_s *max_array = (const json_array_s *)max_value->payload;
                json_array_element_s *max_element = max_array->start;

                while (max_element) {
                    accessor.max.push_back(get_float(max_element->value));
                    max_element = max_element->next;
                }
            }

            json_value_s *name_value = find_member(accessor_obj, "name");
            if (name_value) {
                accessor.name = get_string(name_value);
            }

            accessors.push_back(accessor);
        }

        element = element->next;
    }

    return true;
}

bool Model::load_buffers() {
    for (usize i = 0; i < buffers.size(); i++) {
        if (buffers[i].loaded) {
            // Already loaded (e.g., from GLB binary chunk)
            continue;
        }

        const std::string &uri = buffers[i].uri;

        // Handle data URIs
        if (uri.substr(0, 5) == "data:") {
            usize comma_pos = uri.find(',');
            if (comma_pos != std::string::npos) {
                std::string header = uri.substr(0, comma_pos);
                std::string data = uri.substr(comma_pos + 1);

                // Check if it's base64 encoded
                if (header.find("base64") != std::string::npos) {
                    buffers[i].data = decode_base_64(data);
                } else {
                    // Raw data URI (rare)
                    buffers[i].data.assign(data.begin(), data.end());
                }

                buffers[i].loaded = true;
            }
        } else {
            // External file reference
            std::string file_path = base_path + uri;
            std::ifstream file(file_path, std::ios::binary);

            if (!file) {
                std::cerr << "Failed to open buffer file: " << file_path << std::endl;
                return false;
            }

            // Get file size
            file.seekg(0, std::ios::end);
            usize file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            // Read data
            buffers[i].data.resize(file_size);
            file.read(reinterpret_cast<char *>(buffers[i].data.data()), file_size);

            buffers[i].loaded = true;
        }
    }

    return true;
}

bool Model::parse_images(const json_object_s *json) {
    json_value_s *images_value = find_member(json, "images");
    if (!images_value || images_value->type != json_type_array) return true; // No images, skip

    const json_array_s *images_array = (const json_array_s *)images_value->payload;
    json_array_element_s *element = images_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *image_obj = (const json_object_s *)element->value->payload;

            Image image;

            json_value_s *uri_value = find_member(image_obj, "uri");
            if (uri_value) {
                image.uri = get_string(uri_value);
            }

            json_value_s *mime_type_value = find_member(image_obj, "mimeType");
            if (mime_type_value) {
                image.mime_type = get_string(mime_type_value);
            }

            json_value_s *buffer_view_value = find_member(image_obj, "bufferView");
            if (buffer_view_value) {
                image.buffer_view = get_int(buffer_view_value);
            }

            json_value_s *name_value = find_member(image_obj, "name");
            if (name_value) {
                image.name = get_string(name_value);
            }

            images.push_back(image);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_samplers(const json_object_s *json) {
    json_value_s *samplers_value = find_member(json, "samplers");
    if (!samplers_value || samplers_value->type != json_type_array) return true; // No samplers, skip

    const json_array_s *samplers_array = (const json_array_s *)samplers_value->payload;
    json_array_element_s *element = samplers_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *sampler_obj = (const json_object_s *)element->value->payload;

            Sampler sampler;

            json_value_s *mag_filter_value = find_member(sampler_obj, "magFilter");
            if (mag_filter_value) {
                sampler.mag_filter = get_int(mag_filter_value);
            }

            json_value_s *min_filter_value = find_member(sampler_obj, "minFilter");
            if (min_filter_value) {
                sampler.min_filter = get_int(min_filter_value);
            }

            json_value_s *wrap_s_value = find_member(sampler_obj, "wrapS");
            if (wrap_s_value) {
                sampler.wrap_s = get_int(wrap_s_value);
            } else {
                sampler.wrap_s = 10497; // GL_REPEAT default
            }

            json_value_s *wrap_t_value = find_member(sampler_obj, "wrapT");
            if (wrap_t_value) {
                sampler.wrap_t = get_int(wrap_t_value);
            } else {
                sampler.wrap_t = 10497; // GL_REPEAT default
            }

            json_value_s *name_value = find_member(sampler_obj, "name");
            if (name_value) {
                sampler.name = get_string(name_value);
            }

            samplers.push_back(sampler);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_textures(const json_object_s *json) {
    json_value_s *textures_value = find_member(json, "textures");
    if (!textures_value || textures_value->type != json_type_array) return true; // No textures, that's fine

    const json_array_s *textures_array = (const json_array_s *)textures_value->payload;
    json_array_element_s *element = textures_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *texture_obj = (const json_object_s *)element->value->payload;

            Texture texture;

            json_value_s *sampler_value = find_member(texture_obj, "sampler");
            if (sampler_value) {
                texture.sampler = get_int(sampler_value);
            }

            json_value_s *source_value = find_member(texture_obj, "source");
            if (source_value) {
                texture.source = get_int(source_value);
            }

            json_value_s *name_value = find_member(texture_obj, "name");
            if (name_value) {
                texture.name = get_string(name_value);
            }

            textures.push_back(texture);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_materials(const json_object_s *json) {
    json_value_s *materials_value = find_member(json, "materials");
    if (!materials_value || materials_value->type != json_type_array) return true; // No materials, that's fine

    const json_array_s *materials_array = (const json_array_s *)materials_value->payload;
    json_array_element_s *element = materials_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *material_obj = (const json_object_s *)element->value->payload;

            Material material;

            // Parse PBR Metallic Roughness
            json_value_s *pbr_value = find_member(material_obj, "pbrMetallicRoughness");
            if (pbr_value && pbr_value->type == json_type_object) {
                const json_object_s *pbr_obj = (const json_object_s *)pbr_value->payload;

                // Base color factor
                json_value_s *base_color_factor_value = find_member(pbr_obj, "baseColorFactor");
                if (base_color_factor_value && base_color_factor_value->type == json_type_array) {
                    const json_array_s *factor_array = (const json_array_s *)base_color_factor_value->payload;
                    if (factor_array->length >= 4) {
                        json_array_element_s *r = factor_array->start;
                        json_array_element_s *g = r->next;
                        json_array_element_s *b = g->next;
                        json_array_element_s *a = b->next;

                        material.pbr_metallic_roughness.base_color_factor.x = get_float(r->value);
                        material.pbr_metallic_roughness.base_color_factor.y = get_float(g->value);
                        material.pbr_metallic_roughness.base_color_factor.z = get_float(b->value);
                        material.pbr_metallic_roughness.base_color_factor.w = get_float(a->value);
                    }
                }

                // Base color texture
                json_value_s *base_color_texture_value = find_member(pbr_obj, "baseColorTexture");
                if (base_color_texture_value && base_color_texture_value->type == json_type_object) {
                    const json_object_s *tex_info_obj = (const json_object_s *)base_color_texture_value->payload;

                    json_value_s *index_value = find_member(tex_info_obj, "index");
                    if (index_value) {
                        material.pbr_metallic_roughness.base_color_texture.index = get_int(index_value);
                    }

                    json_value_s *tex_coord_value = find_member(tex_info_obj, "texCoord");
                    if (tex_coord_value) {
                        material.pbr_metallic_roughness.base_color_texture.tex_coord = get_int(tex_coord_value);
                    }
                }

                // Metallic factor
                json_value_s *metallic_factor_value = find_member(pbr_obj, "metallicFactor");
                if (metallic_factor_value) {
                    material.pbr_metallic_roughness.metallic_factor = get_float(metallic_factor_value);
                }

                // Roughness factor
                json_value_s *roughness_factor_value = find_member(pbr_obj, "roughnessFactor");
                if (roughness_factor_value) {
                    material.pbr_metallic_roughness.roughness_factor = get_float(roughness_factor_value);
                }

                // Metallic roughness texture
                json_value_s *metallic_roughness_texture_value = find_member(pbr_obj, "metallicRoughnessTexture");
                if (metallic_roughness_texture_value && metallic_roughness_texture_value->type == json_type_object) {
                    const json_object_s *tex_info_obj =
                        (const json_object_s *)metallic_roughness_texture_value->payload;

                    json_value_s *index_value = find_member(tex_info_obj, "index");
                    if (index_value) {
                        material.pbr_metallic_roughness.metallic_roughness_texture.index = get_int(index_value);
                    }

                    json_value_s *tex_coord_value = find_member(tex_info_obj, "texCoord");
                    if (tex_coord_value) {
                        material.pbr_metallic_roughness.metallic_roughness_texture.tex_coord = get_int(tex_coord_value);
                    }
                }
            }

            // Normal texture
            json_value_s *normal_texture_value = find_member(material_obj, "normalTexture");
            if (normal_texture_value && normal_texture_value->type == json_type_object) {
                const json_object_s *tex_info_obj = (const json_object_s *)normal_texture_value->payload;

                json_value_s *index_value = find_member(tex_info_obj, "index");
                if (index_value) {
                    material.normal_texture.index = get_int(index_value);
                }

                json_value_s *tex_coord_value = find_member(tex_info_obj, "texCoord");
                if (tex_coord_value) {
                    material.normal_texture.tex_coord = get_int(tex_coord_value);
                }
            }

            // Occlusion texture
            json_value_s *occlusion_texture_value = find_member(material_obj, "occlusionTexture");
            if (occlusion_texture_value && occlusion_texture_value->type == json_type_object) {
                const json_object_s *tex_info_obj = (const json_object_s *)occlusion_texture_value->payload;

                json_value_s *index_value = find_member(tex_info_obj, "index");
                if (index_value) {
                    material.occlusion_texture.index = get_int(index_value);
                }

                json_value_s *tex_coord_value = find_member(tex_info_obj, "texCoord");
                if (tex_coord_value) {
                    material.occlusion_texture.tex_coord = get_int(tex_coord_value);
                }
            }

            // Emissive texture
            json_value_s *emissive_texture_value = find_member(material_obj, "emissiveTexture");
            if (emissive_texture_value && emissive_texture_value->type == json_type_object) {
                const json_object_s *tex_info_obj = (const json_object_s *)emissive_texture_value->payload;

                json_value_s *index_value = find_member(tex_info_obj, "index");
                if (index_value) {
                    material.emissive_texture.index = get_int(index_value);
                }

                json_value_s *tex_coord_value = find_member(tex_info_obj, "texCoord");
                if (tex_coord_value) {
                    material.emissive_texture.tex_coord = get_int(tex_coord_value);
                }
            }

            // Emissive factor
            json_value_s *emissive_factor_value = find_member(material_obj, "emissiveFactor");
            if (emissive_factor_value && emissive_factor_value->type == json_type_array) {
                const json_array_s *factor_array = (const json_array_s *)emissive_factor_value->payload;
                if (factor_array->length >= 3) {
                    json_array_element_s *r = factor_array->start;
                    json_array_element_s *g = r->next;
                    json_array_element_s *b = g->next;

                    material.emissive_factor.x = get_float(r->value);
                    material.emissive_factor.y = get_float(g->value);
                    material.emissive_factor.z = get_float(b->value);
                }
            }

            // Alpha mode
            json_value_s *alpha_mode_value = find_member(material_obj, "alphaMode");
            if (alpha_mode_value) {
                material.alpha_mode = get_string(alpha_mode_value);
            } else {
                material.alpha_mode = "OPAQUE";
            }

            // Alpha cutoff
            json_value_s *alpha_cutoff_value = find_member(material_obj, "alphaCutoff");
            if (alpha_cutoff_value) {
                material.alpha_cutoff = get_float(alpha_cutoff_value);
            } else {
                material.alpha_cutoff = 0.5f;
            }

            // Double sided
            json_value_s *double_sided_value = find_member(material_obj, "doubleSided");
            material.double_sided = double_sided_value ? get_bool(double_sided_value) : false;

            // Name
            json_value_s *name_value = find_member(material_obj, "name");
            if (name_value) {
                material.name = get_string(name_value);
            }

            materials.push_back(material);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_meshes(const json_object_s *json) {
    json_value_s *meshes_value = find_member(json, "meshes");
    if (!meshes_value || meshes_value->type != json_type_array) return true; // No meshes, that's fine

    const json_array_s *meshes_array = (const json_array_s *)meshes_value->payload;
    json_array_element_s *element = meshes_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *mesh_obj = (const json_object_s *)element->value->payload;

            Mesh mesh;

            // Primitives
            json_value_s *primitives_value = find_member(mesh_obj, "primitives");
            if (primitives_value && primitives_value->type == json_type_array) {
                const json_array_s *primitives_array = (const json_array_s *)primitives_value->payload;
                json_array_element_s *prim_element = primitives_array->start;

                while (prim_element) {
                    if (prim_element->value->type == json_type_object) {
                        const json_object_s *prim_obj = (const json_object_s *)prim_element->value->payload;

                        Primitive primitive;

                        // Attributes
                        json_value_s *attributes_value = find_member(prim_obj, "attributes");
                        if (attributes_value && attributes_value->type == json_type_object) {
                            const json_object_s *attrs_obj = (const json_object_s *)attributes_value->payload;
                            json_object_element_s *attr_element = attrs_obj->start;

                            while (attr_element) {
                                std::string attr_name(attr_element->name->string, attr_element->name->string_size);
                                i32 accessor_index = get_int(attr_element->value);
                                primitive.attributes[attr_name] = accessor_index;
                                attr_element = attr_element->next;
                            }
                        }

                        // Indices
                        json_value_s *indices_value = find_member(prim_obj, "indices");
                        if (indices_value) {
                            primitive.indices = get_int(indices_value);
                        }

                        // Material
                        json_value_s *material_value = find_member(prim_obj, "material");
                        if (material_value) {
                            primitive.material = get_int(material_value);
                        }

                        // Mode
                        json_value_s *mode_value = find_member(prim_obj, "mode");
                        if (mode_value) {
                            primitive.mode = get_int(mode_value);
                        } else {
                            primitive.mode = 4; // GL_TRIANGLES
                        }

                        mesh.primitives.push_back(primitive);
                    }

                    prim_element = prim_element->next;
                }
            }

            // Weights
            json_value_s *weights_value = find_member(mesh_obj, "weights");
            if (weights_value && weights_value->type == json_type_array) {
                const json_array_s *weights_array = (const json_array_s *)weights_value->payload;
                json_array_element_s *weight_element = weights_array->start;

                while (weight_element) {
                    mesh.weights.push_back(get_float(weight_element->value));
                    weight_element = weight_element->next;
                }
            }

            // Name
            json_value_s *name_value = find_member(mesh_obj, "name");
            if (name_value) {
                mesh.name = get_string(name_value);
            }

            meshes.push_back(mesh);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_skins(const json_object_s *json) {
    json_value_s *skins_value = find_member(json, "skins");
    if (!skins_value || skins_value->type != json_type_array) return true; // No skins, that's fine

    const json_array_s *skins_array = (const json_array_s *)skins_value->payload;
    json_array_element_s *element = skins_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *skin_obj = (const json_object_s *)element->value->payload;

            Skin skin;

            // Inverse bind matrices
            json_value_s *inverse_bind_matrices_value = find_member(skin_obj, "inverseBindMatrices");
            if (inverse_bind_matrices_value) {
                skin.inverse_bind_matrices = get_int(inverse_bind_matrices_value);
            }

            // Skeleton
            json_value_s *skeleton_value = find_member(skin_obj, "skeleton");
            if (skeleton_value) {
                skin.skeleton = get_int(skeleton_value);
            }

            // Joints
            json_value_s *joints_value = find_member(skin_obj, "joints");
            if (joints_value && joints_value->type == json_type_array) {
                const json_array_s *joints_array = (const json_array_s *)joints_value->payload;
                json_array_element_s *joint_element = joints_array->start;

                while (joint_element) {
                    skin.joints.push_back(get_int(joint_element->value));
                    joint_element = joint_element->next;
                }
            }

            // Name
            json_value_s *name_value = find_member(skin_obj, "name");
            if (name_value) {
                skin.name = get_string(name_value);
            }

            skins.push_back(skin);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_nodes(const json_object_s *json) {
    json_value_s *nodes_value = find_member(json, "nodes");
    if (!nodes_value || nodes_value->type != json_type_array) return true; // No nodes, that's fine

    const json_array_s *nodes_array = (const json_array_s *)nodes_value->payload;
    json_array_element_s *element = nodes_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *node_obj = (const json_object_s *)element->value->payload;

            Node node;

            // Children
            json_value_s *children_value = find_member(node_obj, "children");
            if (children_value && children_value->type == json_type_array) {
                const json_array_s *children_array = (const json_array_s *)children_value->payload;
                json_array_element_s *child_element = children_array->start;

                while (child_element) {
                    node.children.push_back(get_int(child_element->value));
                    child_element = child_element->next;
                }
            }

            // Mesh
            json_value_s *mesh_value = find_member(node_obj, "mesh");
            if (mesh_value) {
                node.mesh = get_int(mesh_value);
            }

            // Skin
            json_value_s *skin_value = find_member(node_obj, "skin");
            if (skin_value) {
                node.skin = get_int(skin_value);
            }

            // Check for transformation: matrix OR TRS
            json_value_s *matrix_value = find_member(node_obj, "matrix");
            if (matrix_value && matrix_value->type == json_type_array) {
                const json_array_s *matrix_array = (const json_array_s *)matrix_value->payload;
                if (matrix_array->length == 16) {
                    json_array_element_s *mat_element = matrix_array->start;
                    for (int i = 0; i < 16; i++) {
                        node.matrix.m[i] = get_float(mat_element->value);
                        mat_element = mat_element->next;
                    }
                    node.has_matrix = true;
                }
            } else {
                // Translation
                json_value_s *translation_value = find_member(node_obj, "translation");
                if (translation_value && translation_value->type == json_type_array) {
                    const json_array_s *translation_array = (const json_array_s *)translation_value->payload;
                    if (translation_array->length >= 3) {
                        json_array_element_s *x = translation_array->start;
                        json_array_element_s *y = x->next;
                        json_array_element_s *z = y->next;

                        node.translation.x = get_float(x->value);
                        node.translation.y = get_float(y->value);
                        node.translation.z = get_float(z->value);
                    }
                }

                // Rotation (quaternion)
                json_value_s *rotation_value = find_member(node_obj, "rotation");
                if (rotation_value && rotation_value->type == json_type_array) {
                    const json_array_s *rotation_array = (const json_array_s *)rotation_value->payload;
                    if (rotation_array->length >= 4) {
                        json_array_element_s *x = rotation_array->start;
                        json_array_element_s *y = x->next;
                        json_array_element_s *z = y->next;
                        json_array_element_s *w = z->next;

                        node.rotation.x = get_float(x->value);
                        node.rotation.y = get_float(y->value);
                        node.rotation.z = get_float(z->value);
                        node.rotation.w = get_float(w->value);
                    }
                }

                // Scale
                json_value_s *scale_value = find_member(node_obj, "scale");
                if (scale_value && scale_value->type == json_type_array) {
                    const json_array_s *scale_array = (const json_array_s *)scale_value->payload;
                    if (scale_array->length >= 3) {
                        json_array_element_s *x = scale_array->start;
                        json_array_element_s *y = x->next;
                        json_array_element_s *z = y->next;

                        node.scale.x = get_float(x->value);
                        node.scale.y = get_float(y->value);
                        node.scale.z = get_float(z->value);
                    }
                }
            }

            // Name
            json_value_s *name_value = find_member(node_obj, "name");
            if (name_value) {
                node.name = get_string(name_value);
            }

            nodes.push_back(node);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_scenes(const json_object_s *json) {
    json_value_s *scenes_value = find_member(json, "scenes");
    if (!scenes_value || scenes_value->type != json_type_array) return true; // No scenes, that's fine

    const json_array_s *scenes_array = (const json_array_s *)scenes_value->payload;
    json_array_element_s *element = scenes_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *scene_obj = (const json_object_s *)element->value->payload;

            Scene scene;

            // Nodes
            json_value_s *nodes_value = find_member(scene_obj, "nodes");
            if (nodes_value && nodes_value->type == json_type_array) {
                const json_array_s *nodes_array = (const json_array_s *)nodes_value->payload;
                json_array_element_s *node_element = nodes_array->start;

                while (node_element) {
                    scene.nodes.push_back(get_int(node_element->value));
                    node_element = node_element->next;
                }
            }

            // Name
            json_value_s *name_value = find_member(scene_obj, "name");
            if (name_value) {
                scene.name = get_string(name_value);
            }

            scenes.push_back(scene);
        }

        element = element->next;
    }

    return true;
}

bool Model::parse_animations(const json_object_s *json) {
    json_value_s *animations_value = find_member(json, "animations");
    if (!animations_value || animations_value->type != json_type_array) return true; // No animations, that's fine

    const json_array_s *animations_array = (const json_array_s *)animations_value->payload;
    json_array_element_s *element = animations_array->start;

    while (element) {
        if (element->value->type == json_type_object) {
            const json_object_s *animation_obj = (const json_object_s *)element->value->payload;

            Animation animation;

            // Samplers
            json_value_s *samplers_value = find_member(animation_obj, "samplers");
            if (samplers_value && samplers_value->type == json_type_array) {
                const json_array_s *samplers_array = (const json_array_s *)samplers_value->payload;
                json_array_element_s *sampler_element = samplers_array->start;

                while (sampler_element) {
                    if (sampler_element->value->type == json_type_object) {
                        const json_object_s *sampler_obj = (const json_object_s *)sampler_element->value->payload;

                        AnimationSampler sampler;

                        // Input (keyframe times)
                        json_value_s *input_value = find_member(sampler_obj, "input");
                        if (input_value) {
                            sampler.input = get_int(input_value);
                        }

                        // Output (keyframe values)
                        json_value_s *output_value = find_member(sampler_obj, "output");
                        if (output_value) {
                            sampler.output = get_int(output_value);
                        }

                        // Interpolation
                        json_value_s *interpolation_value = find_member(sampler_obj, "interpolation");
                        if (interpolation_value) {
                            sampler.interpolation = get_string(interpolation_value);
                        } else {
                            sampler.interpolation = "LINEAR";
                        }

                        animation.samplers.push_back(sampler);
                    }

                    sampler_element = sampler_element->next;
                }
            }

            // Channels
            json_value_s *channels_value = find_member(animation_obj, "channels");
            if (channels_value && channels_value->type == json_type_array) {
                const json_array_s *channels_array = (const json_array_s *)channels_value->payload;
                json_array_element_s *channel_element = channels_array->start;

                while (channel_element) {
                    if (channel_element->value->type == json_type_object) {
                        const json_object_s *channel_obj = (const json_object_s *)channel_element->value->payload;

                        AnimationChannel channel;

                        // Sampler
                        json_value_s *sampler_value = find_member(channel_obj, "sampler");
                        if (sampler_value) {
                            channel.sampler = get_int(sampler_value);
                        }

                        // Target
                        json_value_s *target_value = find_member(channel_obj, "target");
                        if (target_value && target_value->type == json_type_object) {
                            const json_object_s *target_obj = (const json_object_s *)target_value->payload;

                            // Node
                            json_value_s *node_value = find_member(target_obj, "node");
                            if (node_value) {
                                channel.target.node = get_int(node_value);
                            }

                            // Path
                            json_value_s *path_value = find_member(target_obj, "path");
                            if (path_value) {
                                channel.target.path = get_string(path_value);
                            }
                        }

                        animation.channels.push_back(channel);
                    }

                    channel_element = channel_element->next;
                }
            }

            // Name
            json_value_s *name_value = find_member(animation_obj, "name");
            if (name_value) {
                animation.name = get_string(name_value);
            }

            animations.push_back(animation);
        }

        element = element->next;
    }

    return true;
}

}; // namespace gltf
