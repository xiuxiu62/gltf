#pragma once

#include "json.hpp"
#include "types.hpp"
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace gltf {

struct Vec2 {
    f32 x, y;
};

struct Vec3 {
    f32 x, y, z;

    static constexpr Vec3 zero() {
        return {0.0f, 0.0f, 0.0f};
    }

    static constexpr Vec3 one() {
        return {1.0f, 1.0f, 1.0f};
    }
};

struct Vec4 {
    f32 x, y, z, w;

    static constexpr Vec4 zero() {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    static constexpr Vec4 one() {
        return {1.0f, 1.0f, 1.0f, 1.0f};
    }
};

struct Mat4 {
    union {
        f32 m[16];
        f32 cols[4][4];
    };

    static constexpr Mat4 identify() {
        return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    }
};

struct Buffer {
    std::string uri;
    u64 byte_length = 0;
    std::vector<u8> data;
    bool loaded = false;
};

struct BufferView {
    u32 buffer = 0;
    usize byte_offset = 0;
    usize byte_length = 0;
    usize byte_stride = 0;
    i32 target = 0;
    std::string name;
};

struct Accessor {
    u32 buffer_view = 0;
    u64 byte_offset = 0;
    i32 component_type = 0; // GL_BYTE, GL_UNSIGNED_BYTE, etc.
    bool normalized = false;
    u64 count = 0;
    std::string type; // "SCALAR", "VEC2", "VEC3", etc.
    std::vector<f32> min, max;
    std::string name;
};

struct Image {
    std::string uri;
    std::string mime_type;
    u32 buffer_view = 0;
    std::string name;

    // Storage for loaded image data
    std::vector<u8> data;
    i32 width = 0;
    i32 height = 0;
    i32 channels = 0;
    bool loaded = false;
};

struct Sampler {
    i32 mag_filter = 0; // GL_LINEAR, GL_NEAREST
    i32 min_filter = 0; // GL_LINEAR_MIPMAP_LINEAR, etc.
    i32 wrap_s = 10497; // GL_REPEAT (10497)
    i32 wrap_t = 10497; // GL_REPEAT (10497)
    std::string name;
};

struct Texture {
    u32 sampler = 0;
    u32 source = 0;
    std::string name;
};

struct TextureInfo {
    u32 index = 0;
    u32 tex_coord = 0;
};

struct PbrMetallicRoughness {
    Vec4 base_color_factor = Vec4::one();
    TextureInfo base_color_texture;
    f32 metallic_factor = 1.0f;
    f32 roughness_factor = 1.0f;
    TextureInfo metallic_roughness_texture;
};

struct Material {
    PbrMetallicRoughness pbr_metallic_roughness;
    TextureInfo normal_texture;
    TextureInfo occlusion_texture;
    TextureInfo emissive_texture;
    Vec3 emissive_factor = Vec3::zero();
    std::string alpha_mode = "OPAQUE"; // "OPAQUE", "MASK", or "BLEND"
    f32 alpha_cutoff = 0.5f;
    bool double_sided = false;
    std::string name;
};

struct Primitive {
    std::map<std::string, u32> attributes;
    u32 indices = UINT32_MAX;
    u32 material = UINT32_MAX;
    i32 mode = 4; // GL_TRIANGLES (4) by default
};

struct Mesh {
    std::vector<Primitive> primitives;
    std::vector<f32> weights;
    std::string name;
};

struct Skin {
    u32 inverse_bind_matrices = UINT32_MAX;
    u32 skeleton = UINT32_MAX;
    std::vector<u32> joints;
    std::string name;
};

struct Node {
    std::vector<u32> children;
    u32 mesh = UINT32_MAX;
    u32 skin = UINT32_MAX;

    Mat4 matrix = Mat4::identify();
    bool has_matrix = false;

    Vec3 translation = Vec3::zero();
    Vec4 rotation = {0, 0, 0, 1};
    Vec3 scale = Vec3::one();

    std::string name;
};

struct Scene {
    std::vector<u32> nodes;
    std::string name;
};

struct AnimationSampler {
    u32 input = UINT32_MAX;               // Accessor for keyframe times
    u32 output = UINT32_MAX;              // Accessor for keyframe values
    std::string interpolation = "LINEAR"; // "LINEAR", "STEP", "CUBICSPLINE"
};

struct AnimationChannel {
    u32 sampler = 0;
    struct Target {
        u32 node = UINT32_MAX;
        std::string path;
    } target;
};

struct Animation {
    std::vector<AnimationSampler> samplers;
    std::vector<AnimationChannel> channels;
    std::string name;
};

struct Model {
    std::vector<Buffer> buffers;
    std::vector<BufferView> buffer_views;
    std::vector<Accessor> accessors;
    std::vector<Image> images;
    std::vector<Sampler> samplers;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<Skin> skins;
    std::vector<Node> nodes;
    std::vector<Scene> scenes;
    std::vector<Animation> animations;

    u32 default_scene = 0;

    // Base path for resolving external files
    std::string base_path;

    static Model load(bool &success, const std::string &path);
    static Model load_from_memory(bool &success, const u8 *data, usize length);
    bool save(const std::string &path);
    bool save_as_gltf(const std::string &path, bool embed_buffers);
    bool save_as_glb(const std::string &path);
    // void destroy();

  private:
    bool parse(const json_value_s *root);
    std::string generate_json(bool for_glb);

    bool load_buffers();

    bool parse_buffers(const json_object_s *json);
    bool parse_buffer_views(const json_object_s *json);
    bool parse_accessors(const json_object_s *json);
    bool parse_images(const json_object_s *json);
    bool parse_samplers(const json_object_s *json);
    bool parse_textures(const json_object_s *json);
    bool parse_materials(const json_object_s *json);
    bool parse_meshes(const json_object_s *json);
    bool parse_skins(const json_object_s *json);
    bool parse_nodes(const json_object_s *json);
    bool parse_scenes(const json_object_s *json);
    bool parse_animations(const json_object_s *json);

    static std::string get_string(const json_value_s *value);
    static i32 get_int(const json_value_s *value);
    static f32 get_float(const json_value_s *value);
    static bool get_bool(const json_value_s *value);
};

}; // namespace gltf
