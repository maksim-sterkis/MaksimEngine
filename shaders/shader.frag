#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 0) uniform sampler2D texSamplers[];

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec2 fragUV_X;
layout(location = 5) in vec2 fragUV_Y;
layout(location = 6) in vec2 fragUV_Z;

layout(location = 7) in flat int fragPrimitiveID;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 colorOverride;
    int useOverride;
    int useTriplanar;
    int hasTexture;
    int debugColors;
    int textureIndex;
    int padding[7];
} push;

void main() {
    if (push.debugColors == 1) {
        float r = fract(sin(float(fragPrimitiveID) * 12.9898) * 43758.5453);
        float g = fract(sin(float(fragPrimitiveID) * 78.233) * 43758.5453);
        float b = fract(sin(float(fragPrimitiveID) * 39.346) * 43758.5453);
        outColor = vec4(r, g, b, 1.0);
        return;
    }

    vec4 texColor = vec4(1.0);
    
    if (push.hasTexture == 1) {
        if (push.useTriplanar == 1) {
            vec3 blendWeights = abs(normalize(fragNormal));
            blendWeights = max(blendWeights - 0.2, 0.0);
            float sum = blendWeights.x + blendWeights.y + blendWeights.z;
            if (sum > 0.00001) {
                blendWeights /= sum;
            } else {
                blendWeights = vec3(1.0, 0.0, 0.0);
            }
            
            vec4 colX = vec4(0.0);
            vec4 colY = vec4(0.0);
            vec4 colZ = vec4(0.0);
            
            if (blendWeights.x > 0.0) colX = texture(texSamplers[nonuniformEXT(push.textureIndex)], fragUV_X);
            if (blendWeights.y > 0.0) colY = texture(texSamplers[nonuniformEXT(push.textureIndex)], fragUV_Y);
            if (blendWeights.z > 0.0) colZ = texture(texSamplers[nonuniformEXT(push.textureIndex)], fragUV_Z);
            
            texColor = colX * blendWeights.x + colY * blendWeights.y + colZ * blendWeights.z;
        } else {
            texColor = texture(texSamplers[nonuniformEXT(push.textureIndex)], fragUV);
        }
    }
    
    vec3 baseColor = fragColor;
    if (push.hasTexture == 1) {
        baseColor = vec3(1.0);
    }
    outColor = vec4(baseColor, 1.0) * texColor;
}
