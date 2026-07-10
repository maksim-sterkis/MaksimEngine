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

## Phase 3 (Meshlets & Visibility Buffer)
- **Status**: ⏳ **PENDING**
- **Goal**: Replace legacy vertex pipelines with Task/Mesh shaders (`VK_EXT_mesh_shader`).
- **Why**: Unlocks extreme GPU geometry culling (frustum, occlusion) at a sub-mesh granularity.
- **Tasks**:
  1. Write a compute shader or offline tool extension to partition `ModelData` into meshlets (max 64 vertices, 126 triangles).
  2. Create Task and Mesh shaders to evaluate and output geometry.
  3. Switch to a two-pass Visibility Buffer workflow to decouple geometry from heavy material evaluation.

## Phase 4 (Hardware Ray Tracing - RT Pipeline)
- **Status**: ⏳ **PENDING**
- **Goal**: Add hardware-accelerated ray tracing via `VK_KHR_ray_tracing_pipeline`.
- **Why**: Leverage the RTX 3090's RT cores for precise shadowing, ambient occlusion, and reflections.
- **Tasks**:
  1. Build a Bottom-Level Acceleration Structure (BLAS) for every static mesh, and a Top-Level Acceleration Structure (TLAS) for scene instances.
  2. Implement Ray Generation, Closest Hit, and Miss shaders to trace scene intersections.
  3. Write a compute shader or RT pipeline pass to evaluate direct shadowing using ray queries against the TLAS.

## Phase 5 (Next-Gen Lighting - ReSTIR & Denoising)
- **Status**: ⏳ **PENDING**
- **Goal**: Implement ReSTIR DI (Direct Illumination) and GI (Global Illumination), alongside an SVGF denoiser.
- **Why**: Provides real-time path-traced lighting quality efficiently.
- **Tasks**:
  1. Implement ReSTIR DI to efficiently sample many light sources.
  2. Implement ReSTIR GI for infinite-bounce diffuse interreflection.
  3. Implement an SVGF (Spatiotemporal Variance-Guided Filter) denoiser to clean up the stochastic RT noise.
