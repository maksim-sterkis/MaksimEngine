# VK Game Engine

A modern Vulkan game engine written in C++20, designed with next-generation rendering techniques in mind.

## Features

- **Bindless Architecture**: Utilizes Vulkan 1.2 Descriptor Indexing (`VK_EXT_descriptor_indexing`) to drastically reduce CPU overhead during draw calls. An unbounded array of 100,000 descriptors allows for virtually limitless textures and materials mapped directly via PushConstants.
- **Offline Asset Compiler**: Custom asset pipeline utilizing `fastgltf` and `tinyobjloader` to process `.obj` and `.gltf` source files into optimized, single-binary `.glb` payloads.
- **Embedded Textures**: The compiler natively reads raw PBR texture files (JPEGs/PNGs) and packages them dynamically into the `.glb` buffers, resulting in clean, self-contained mesh payloads.
- **Dynamic Asset Pool**: Robust texture and model pooling system preventing duplicate GPU uploads and seamlessly switching between raw JPEG/PNG loading (using `stb_image`) and compressed formats.
- **PBR Materials**: Complete physical based rendering foundation with Cook-Torrance BRDF (Albedo, Normal, Metallic, Roughness) via SSBOs.

## Roadmap & Upcoming Features

1. **Mesh Shaders & Visibility Buffer**: Transition to `VK_EXT_mesh_shader` for high-performance sub-mesh geometry culling.
2. **Hardware Ray Tracing**: Leverage `VK_KHR_ray_tracing_pipeline` (RT cores) for precise shadows, reflections, and ambient occlusion.
3. **ReSTIR DI / GI**: State-of-the-art reservoir spatiotemporal importance resampling for real-time direct and global illumination.

## Build Instructions

### Prerequisites
- **CMake** 3.20+
- **C++20** compliant compiler (GCC/MinGW, MSVC, or Clang)
- **Vulkan SDK** 1.2+

### Building

The project uses CMake to fetch dependencies (like `fastgltf`, `glfw`, `glm`, `imgui`) automatically.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Running

To run the offline model compiler to package your assets (put source models in `assets/models/obj`):
```bash
./build/model_compiler.exe
```

To run the engine itself:
```bash
./build/VK_game_engine.exe
```

## Credits & Dependencies
- [Vulkan](https://www.vulkan.org/)
- [fastgltf](https://github.com/spnda/fastgltf)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
- [stb_image](https://github.com/nothings/stb)
- [GLFW](https://www.glfw.org/)
- [GLM](https://github.com/g-truc/glm)
- [Dear ImGui](https://github.com/ocornut/imgui)


## Pictures

<img width="2251" height="1185" alt="Screenshot 2026-07-09 175616" src="https://github.com/user-attachments/assets/225f0c4a-3c32-4cc7-9cf0-d51cf35c93ab" />

