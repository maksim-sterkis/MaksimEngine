# Vulkan Engine Roadmap

This plan outlines the next major architectural upgrades for the engine, focusing on bindless textures, PBR materials, advanced culling, hardware ray tracing, and next-gen lighting.

## Phase 0 (Asset Pipeline & KTX2 Compression)
- **Status**: ✅ **COMPLETED**
- **Goal**: Built an offline model compiler that parses `.gltf` and `.obj` files using `fastgltf` and `tinyobjloader`, compresses raw textures into `KTX2 UASTC` format using `toktx`, and packages them natively into optimized binary `.glb` files.
- **Why**: Allows the engine to load massive Poly Haven PBR assets instantly, streaming pre-compressed textures directly to VRAM in `BC7` format to save gigabytes of memory.

## Phase 0.5 (Fix GLB Texture Embedding)
- **Status**: ✅ **COMPLETED**
- **Goal**: Ensure the textures are properly embedded into the `.glb` binary chunk rather than saving as external URIs.
- **Why**: The debug logs revealed `index=2` (`fastgltf::sources::URI`), which means `fastgltf::Exporter` is trying to write the KTX2 textures out as separate files instead of packing them into the `.glb` binary! This is why `ktxTexture2_CreateFromMemory` is failing—the byte array is empty because the data isn't in the buffer chunk. We will fix `tools/model_compiler.cpp` to properly serialize the textures into a `BufferView` within the GLB so it behaves as a single self-contained file.

## Phase 1 (Bindless Architecture & SSBOs)
- **Status**: ✅ **COMPLETED**
- **Goal**: Transition from tight per-draw descriptor binding to global descriptor arrays (Bindless) and SSBO-driven material properties.
- **Why**: Eliminates CPU overhead when drawing thousands of objects. Crucial for upcoming Mesh Shaders and Ray Tracing which require arbitrary access to vertex and material data globally.
- **Tasks Completed**:
  1. Added `VK_EXT_descriptor_indexing` (Vulkan 1.2 Descriptor Indexing) to Device features.
  2. Created a Global Descriptor Set containing unbounded arrays of `sampler2D` and SSBOs for Vertex/Index data.
  3. Modified shaders to accept a `MaterialID` and `MeshID` via PushConstants to index into the global SSBOs/Textures, removing texture bindings from the `vkCmdBindDescriptorSets` loop entirely.

## Phase 2 (PBR Materials)
- **Status**: ✅ **COMPLETED**
- **Goal**: Implement physical based rendering (Cook-Torrance BRDF) evaluating Albedo, Normal, Metallic, and Roughness maps.
- **Why**: Required to accurately represent realistic assets like the Poly Haven rock, calculating specular reflections and energy conservation correctly.
- **Tasks Completed**:
  1. Updated Material SSBO to include PBR coefficients and correctly aligned memory using `std140` block limits.
  2. Implemented Schlick-GGX, Smith Geometry, and Fresnel equations in `shader.frag`.
  3. Extracted and mapped multi-texture PBR payloads from GLB models dynamically to the Bindless array.

## Phase 3 (Meshlets & Advanced Culling)
- **Status**: ✅ **COMPLETED**
- **Goal**: Replace legacy vertex pipelines with Task/Mesh shaders (`VK_EXT_mesh_shader`) featuring sub-mesh level Frustum, Cone Backface, and Hi-Z Occlusion Culling.
- **Why**: Unlocks extreme GPU geometry culling at a sub-mesh granularity, evaluating thousands of meshlets in fractions of a millisecond.
- **Tasks Completed**:
  1. Offline tool extension to partition `ModelData` into meshlets (max 64 vertices, 124 triangles) using `meshoptimizer`.
  2. Create Task and Mesh shaders to evaluate bounding spheres, extract frustum planes, and emit visible meshlets.
  3. Generate and bind a multi-mip Hi-Z depth pyramid for mathematically perfect, conservative occlusion culling.
  4. Implement ultra-fast SSBO atomic counters to safely track evaluated vs. drawn meshlets without triggering Nvidia driver bottlenecks.

## Phase 3.5 (Visibility Buffer)
- **Status**: ⏳ **PENDING**
- **Goal**: Transition the engine to a true Visibility Buffer architecture.
- **Why**: Decouples geometry rasterization from heavy material evaluation. By outputting only a 64-bit payload per pixel, we achieve absolute zero overdraw for complex PBR materials—critical for rendering millions of sub-pixel triangles (a la Nanite).
- **Tasks**:
  1. Enable `VK_KHR_fragment_shader_barycentric`.
  2. Create a Visibility Buffer render target (e.g., `R32G32_UINT`).
  3. Modify the Mesh Shader to only output `InstanceID`, `MeshletID`, and `TriangleID` to the screen.
  4. Create a fullscreen compute pass (`deferred.comp`) to read the Visibility Buffer, manually fetch the 3 vertices via SSBOs, compute barycentrics, interpolate UVs/Normals, and execute the PBR shading.

## Phase 4 (Hardware Ray Tracing - RT Pipeline)
- **Status**: ⏳ **PENDING**
- **Goal**: Add hardware-accelerated ray tracing via `VK_KHR_ray_tracing_pipeline`.
- **Why**: Leverage the RTX 3090's RT cores for precise shadowing, ambient occlusion, and reflections.
- **Tasks**:
  1. Build a Bottom-Level Acceleration Structure (BLAS) for every static mesh, and a Top-Level Acceleration Structure (TLAS) for scene instances.
  2. Implement Ray Generation, Closest Hit, and Miss shaders to trace scene intersections.
  3. Write an RT pass to evaluate direct shadowing using ray queries against the TLAS.

## Phase 5 (Next-Gen Lighting - ReSTIR & Denoising)
- **Status**: ⏳ **PENDING**
- **Goal**: Implement ReSTIR DI (Direct Illumination) and GI (Global Illumination), alongside an SVGF denoiser.
- **Why**: Provides real-time path-traced lighting quality efficiently.
- **Tasks**:
  1. Implement ReSTIR DI to efficiently sample many light sources.
  2. Implement ReSTIR GI for infinite-bounce diffuse interreflection.
  3. Implement an SVGF (Spatiotemporal Variance-Guided Filter) denoiser to clean up the stochastic RT noise.
