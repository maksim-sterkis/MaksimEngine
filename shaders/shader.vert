#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragPos;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 colorOverride;
    int useOverride;
} push;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);
    
    if (push.useOverride == 1) {
        fragColor = push.colorOverride.rgb;
    } else {
        fragColor = inColor;
    }
    
    fragPos = inPosition.xy;
}
