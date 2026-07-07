#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec2 fragUV_X;
layout(location = 5) out vec2 fragUV_Y;
layout(location = 6) out vec2 fragUV_Z;

layout(location = 7) out flat int fragPrimitiveID;

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
    gl_Position = push.mvp * vec4(inPosition, 1.0);
    
    if (push.useOverride == 1) {
        fragColor = push.colorOverride.rgb;
    } else {
        fragColor = inColor;
    }
    
    fragNormal = inNormal;
    fragUV = inUV;
    fragWorldPos = inPosition;

    float scale = 0.5;
    fragUV_X = inPosition.yz * scale + vec2(0.5);
    fragUV_Y = inPosition.xz * scale + vec2(0.5);
    fragUV_Z = inPosition.xy * scale + vec2(0.5);
    fragPrimitiveID = gl_VertexIndex / 3;
}
