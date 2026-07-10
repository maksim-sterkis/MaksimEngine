# File Architecture and Connections

This document explains **every single file** in the `VK_game_engine` project and how they interact to form a complete Vulkan engine.

## `game/` (Entry Point & Gameplay)
- **`game/main.cpp`**: The primary entry point. Initializes the engine via `engine::init()`, loads assets (`asset_pool`), creates ECS entities (`MeshRenderer`, `Transform`), updates logic, and drives the `draw_scene` rendering loop.

## `src/` (Core Engine Framework)
### Vulkan Setup & Hardware
- **`src/device.hpp` / `.cpp`**: Handles low-level GPU initialization. Creates the Vulkan `VkInstance`, queries and selects the `VkPhysicalDevice` (GPU), creates the logical `VkDevice`, sets up queue families, and creates the Command Pool.
- **`src/swapchain.hpp` / `.cpp`**: Manages the Vulkan Swapchain (the array of images presented to the screen). Handles querying surface capabilities, selecting present modes (Vsync/Mailbox), and recreating the swapchain when the window resizes.
- **`src/engine.hpp` / `.cpp`**: The central orchestrator for the core framework. It bundles the `Device`, `Window`, and `Swapchain` together. It provides `engine::init()` to boot the system, `engine::run()` to execute the main loop, and synchronizes the CPU and GPU (using fences and semaphores).
- **`src/pipeline.hpp` / `.cpp`**: Defines the Graphics Pipeline. It compiles SPIR-V shaders, configures the rasterizer, depth-stencil, and multi-sampling. Crucially, it sets up the **Bindless Descriptor Set Layouts** (Binding 0 for SSBOs, Binding 1 for dynamic textures) and defines the `PushConstantData` struct.

### Windowing & Input
- **`src/window.hpp` / `.cpp`**: An abstraction over the GLFW library. Creates the OS window, handles fullscreen/borderless toggling, registers resize callbacks, and tracks the window surface.
- **`src/input.hpp` / `.cpp`**: GLFW input system wrapper. Tracks keyboard key states and mouse cursor positioning, allowing `main.cpp` and `camera.cpp` to respond to user actions.

### Utilities & Logic
- **`src/ecs.hpp` / `.cpp`**: A custom, lightweight Entity Component System (ECS). Uses `entt`-style sparse sets to manage `Entity` IDs and their attached components (`Transform`, `MeshRenderer`).
- **`src/camera.hpp` / `.cpp`**: Manages the 3D camera. Uses the `input` system to move around (WASD/Mouse) and calculates the View Matrix sent to the shaders via PushConstants.
- **`src/imgui.hpp` / `.cpp`**: Integration of the Dear ImGui library with Vulkan. Sets up the ImGui context, allocates its own descriptor pool, and handles rendering the debug UI overlay on top of the game.

### Assets & Bindless Resources
- **`src/asset_pool.hpp` / `.cpp`**: The heart of the engine's Bindless architecture. It acts as a global registry holding all loaded `ModelData` and `TextureData`. It compiles the massive `MaterialSSBO` (Storage Buffer) and writes all texture samplers into the single Global Descriptor Set array.
- **`src/model.hpp` / `.cpp`**: Uses `fastgltf` to parse binary `.glb` files. Allocates Vulkan Vertex and Index Buffers, pushes vertices, and extracts PBR Material data (base color, roughness, metallic, normal indices) to be fed back into the `asset_pool`.
- **`src/texture.hpp` / `.cpp`**: Responsible for decoding images into VRAM. It can decode standard formats (PNG/JPG) from memory using `stb_image` and transition them correctly to `VK_FORMAT_R8G8B8A8_UNORM` layouts. It also creates the `VkSampler` objects.

## `shaders/` (GPU Programs)
- **`shaders/shader.vert`**: The Vertex Shader. Multiplies the incoming vertex positions by the `mvp` matrix from PushConstants. Passes `fragUV` and `fragNormal` to the fragment shader.
- **`shaders/shader.frag`**: The Fragment Shader. Uses `push.materialIndex` to read the exact PBR coefficients from the `MaterialSSBO`. It samples the textures directly from the unbounded bindless array and applies the Cook-Torrance BRDF lighting math (Albedo, Normal mapping, Metallic/Roughness reflections).

## `tools/` (Offline Asset Pipeline)
- **`tools/model_compiler.cpp`**: A standalone offline executable. It reads raw `.gltf` or `.obj` files, compresses their raw PNG/JPG textures into highly optimized formats (like KTX2 using `toktx`), and re-packages everything into a single, dense `.glb` binary payload.
- **`tools/texture_compiler.cpp`**: A legacy/helper script used exclusively to run batch conversions of raw texture directories into `KTX2` format.

---

## How They Connect (The Pipeline Flow)

1. **Bootstrapping**: `main.cpp` calls `engine::init()`, which initializes `window`, `device`, `swapchain`, and `pipeline` in that specific order.
2. **Asset Loading**: `main.cpp` asks the `asset_pool` to load a model. The `asset_pool` delegates this to `model::load_glb()`. If the `.glb` has embedded textures, `model.cpp` extracts the bytes and calls `texture::load_image_from_memory()` to upload them to the GPU.
3. **Bindless Sync**: Before entering the game loop, `asset_pool::build_materials_ssbo()` is executed. It aggregates all materials from all loaded models, packs them into a single GPU SSBO, and binds the global texture array to the Descriptor Set created by `pipeline`.
4. **Logic Update**: In the loop, `input` processes keyboard states, updating `camera` to generate a new View matrix. `ecs` is queried for all active `Transform` components to generate Model matrices.
5. **Rendering Pass**: 
   - `engine.cpp` acquires the next `swapchain` image and begins a command buffer.
   - `main.cpp` binds the Global Descriptor Set.
   - For every entity with a `MeshRenderer`, `main.cpp` calculates the `MVP` matrix and pushes it (along with `materialIndex`) to the GPU via `PushConstants`.
   - The GPU executes `shader.vert` and `shader.frag`, referencing the global SSBO and Bindless textures to draw the frame.
   - Finally, `imgui` draws the debug overlay, and `engine.cpp` submits the command buffer to present to the screen.
