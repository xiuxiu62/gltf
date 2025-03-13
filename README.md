# glTF Parser

A lightweight C++11 library for parsing and serializing glTF 2.0 files.

## Features

- Load and parse glTF 2.0 files (.gltf and .glb formats)
- Save models back to glTF or GLB format
- No external dependencies (uses single-header json.h <https://github.com/sheredom/json.h> for JSON parsing)
- Designed for game engines and 3D applications
- Supports all major glTF features:
  - Scene hierarchy with nodes
  - Meshes with primitives
  - Materials (PBR Metallic-Roughness workflow)
  - Textures, Images, and Samplers
  - Animations
  - Skins for skeletal animation

## Usage

### Loading a Model

```cpp
#include "gltf.hpp"

// Load from file
bool success;
Model model = Model::load(success, "path/to/model.gltf");
if (!success) {
    // Handle error
}

// Access the model data
std::cout << "Model has " << model.meshes.size() << " meshes" << std::endl;
std::cout << "Model has " << model.materials.size() << " materials" << std::endl;
```

### Saving a Model

```cpp
// Save as glTF
bool embed_buffers = false; // Set to true to embed binary data as base64
bool success = model.save_as_gltf("output.gltf", embed_buffers);

// Save as binary glTF
bool success = model.save_as_glb("output.glb");

// Auto-detect format based on extension
bool success = model.save("output.gltf"); // Saves as glTF
bool success = model.save("output.glb");  // Saves as GLB
```
