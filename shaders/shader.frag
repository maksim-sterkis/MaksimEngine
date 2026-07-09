#version 450
#extension GL_EXT_nonuniform_qualifier : enable

struct Material {
    int albedoTexIndex;
    int normalTexIndex;
    int metallicRoughnessTexIndex;
    int padding;
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    vec2 padding2;
};

layout(std140, binding = 0) readonly buffer MaterialSSBO {
    Material materials[];
};

layout(binding = 1) uniform sampler2D texSamplers[];

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
    uint materialIndex;
    int padding[7];
} push;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    if (push.debugColors == 1) {
        float r = fract(sin(float(fragPrimitiveID) * 12.9898) * 43758.5453);
        float g = fract(sin(float(fragPrimitiveID) * 78.233) * 43758.5453);
        float b = fract(sin(float(fragPrimitiveID) * 39.346) * 43758.5453);
        outColor = vec4(r, g, b, 1.0);
        return;
    }

    Material mat = materials[push.materialIndex];

    vec4 albedoColor = mat.baseColorFactor;
    if (mat.albedoTexIndex > 0) {
        albedoColor *= texture(texSamplers[nonuniformEXT(mat.albedoTexIndex)], fragUV);
    } else if (mat.albedoTexIndex == 0) { // Default texture fallback if not specified
        // 0 is the fallback white texture, but let's just keep it as baseColorFactor
    }
    
    if (push.useOverride == 1) {
        albedoColor = push.colorOverride;
    }
    vec3 albedo = albedoColor.rgb;

    float metallic = clamp(mat.metallicFactor, 0.0, 1.0);
    float roughness = clamp(mat.roughnessFactor, 0.04, 1.0);

    if (mat.metallicRoughnessTexIndex > 0) {
        vec4 mrSample = texture(texSamplers[nonuniformEXT(mat.metallicRoughnessTexIndex)], fragUV);
        roughness = clamp(roughness * mrSample.g, 0.04, 1.0);
        metallic = clamp(metallic * mrSample.b, 0.0, 1.0);
    }

    vec3 N = normalize(fragNormal);
    if (mat.normalTexIndex > 0) {
        vec3 normalSample = texture(texSamplers[nonuniformEXT(mat.normalTexIndex)], fragUV).rgb;
        
        vec3 Q1 = dFdx(fragWorldPos);
        vec3 Q2 = dFdy(fragWorldPos);
        vec2 st1 = dFdx(fragUV);
        vec2 st2 = dFdy(fragUV);

        vec3 T = Q1*st2.t - Q2*st1.t;
        if (length(T) > 0.0001) {
            T = normalize(T);
            vec3 B = -normalize(cross(N, T)); // negative because Vulkan Y is down
            mat3 TBN = mat3(T, B, N);

            vec3 tnorm = normalSample * 2.0 - 1.0;
            N = normalize(TBN * tnorm);
        }
    }

    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 lightColor = vec3(2.0); 

    vec3 camPos = vec3(0.0, 0.0, 3.0);
    vec3 V = normalize(camPos - fragWorldPos);
    
    // Prevent exactly opposite V and N causing NaN in some reflections
    if (dot(N, V) < 0.0) {
        N = -N;
    }

    vec3 L = lightDir;
    vec3 H = normalize(V + L);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 radiance = lightColor;

    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	  

    float NdotL = max(dot(N, L), 0.0);        
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * albedo * mat.baseColorFactor.rgb; 
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(albedo, albedoColor.a);
}
