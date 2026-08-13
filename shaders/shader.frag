#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) flat in uint inMaterialIndex;

layout(location = 0) out vec4 outColor;

struct Material {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int albedoTexIndex;
    int normalTexIndex;
    int metallicRoughnessTexIndex;
    int padding[3];
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialSSBO {
    Material materials[];
} matBuffer;

layout(set = 1, binding = 1) uniform sampler2D textures[]; // Unbounded array

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 cull_mvp;
    vec4 cull_local_camera_pos;
    vec4 colorOverride;
    int useOverride;
    int useTriplanar;
    int hasTexture;
    int debugColors;
    uint materialIndex;
    uint meshletCount;
} push;

void main() {
    if (push.useOverride == 1 || push.debugColors == 1) {
        outColor = vec4(inColor, 1.0);
        return;
    }
    
    vec4 color = vec4(1.0);
    if (inMaterialIndex < 10000) {
        Material mat = matBuffer.materials[inMaterialIndex];
        color *= mat.baseColorFactor;
        
        if (mat.albedoTexIndex >= 0) {
            int safeIndex = max(mat.albedoTexIndex, 0);
            color *= texture(textures[nonuniformEXT(safeIndex)], inTexCoord);
        }
    }
    
    outColor = vec4(color.rgb * inColor, color.a);
}


