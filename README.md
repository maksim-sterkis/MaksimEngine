# VK Game Engine

A modern Vulkan game engine written in C++20, designed with next-generation rendering techniques in mind.

## Features

- **Bindless Architecture**: Utilizes Vulkan 1.2 Descriptor Indexing (`VK_EXT_descriptor_indexing`) to drastically reduce CPU overhead during draw calls. An unbounded array of 100,000 descriptors allows for virtually limitless textures and materials mapped directly via PushConstants.
- **Offline Asset Compiler**: Custom asset pipeline utilizing `fastgltf` and `tinyobjloader` to process `.obj` and `.gltf` source files into optimized, single-binary `.glb` payloads.
- **Embedded Textures & Meshlets**: The compiler natively reads raw PBR texture files (JPEGs/PNGs) and packages them dynamically into the `.glb` buffers, and uses `meshoptimizer` to append partitioned Meshlets (vertex/triangle arrays) for high-performance geometry culling.
- **Dynamic Asset Pool**: Robust texture and model pooling system preventing duplicate GPU uploads and seamlessly switching between raw JPEG/PNG loading (using `stb_image`) and compressed formats.
- **PBR Materials**: Complete physical based rendering foundation with Cook-Torrance BRDF (Albedo, Normal, Metallic, Roughness) via SSBOs.
- **Mesh Shaders & Advanced Culling**: Uses `VK_EXT_mesh_shader` for sub-mesh geometry culling (Frustum, Cone Backface, and Hi-Z Occlusion) driven entirely on the GPU.
- **Perfect Memory Packing**: Uses `GL_EXT_scalar_block_layout` to map C++ structs exactly to GPU memory without any padding overhead.

## Roadmap & Upcoming Features

1. **Visibility Buffer**: Decouple geometry rasterization from heavy material evaluation by outputting ID buffers and shading via a compute pass.
2. **Hardware Ray Tracing**: Leverage `VK_KHR_ray_tracing_pipeline` (RT cores) for precise shadows, reflections, and ambient occlusion.
3. **ReSTIR DI / GI**: State-of-the-art reservoir spatiotemporal importance resampling for real-time direct and global illumination.

Check out the [full roadmap](roadmap.md) for detailed progress and upcoming milestones.

## Code Architecture

For a complete breakdown of every directory, component, and the GPU pipeline flow, see the [File Architecture Documentation](file.md).

## Images
<img width="2251" height="1185" alt="Screenshot 2026-07-09 175616" src="https://github.com/user-attachments/assets/51e2e0f5-ec02-458e-b2f3-72a4e18ddf5e" />

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
- [meshoptimizer](https://github.com/zeux/meshoptimizer)
- [stb_image](https://github.com/nothings/stb)
- [GLFW](https://www.glfw.org/)
- [GLM](https://github.com/g-truc/glm)
- [Dear ImGui](https://github.com/ocornut/imgui)
