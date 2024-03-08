STRINGIFY(

in vec3 rPos;
in vec3 rNormal;
flat in vec3 rAlbedo;
flat in float rRoughness;
flat in float rMetallic;

out vec4 FragColor;

layout(location = 2) uniform vec3 viewPos;
layout(location = 3) uniform vec3 lightPos;
layout(location = 4) uniform vec3 lightColor;

const float PI = 3.14159265359f;

float DistributionGGX(float NdotH, float roughness) {
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.f) + 1.f;
    denom = PI * denom * denom;
    return alpha2 / max(denom, 0.0000001f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    const float r = roughness + 1.f;
    const float k = (r * r) / 8.f;
    const float ggx1 = NdotV / (NdotV * (1.f - k) + k);
    const float ggx2 = NdotL / (NdotL * (1.f - k) + k);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float HdotV, vec3 baseReflectivity) {
    return baseReflectivity - (1.f - baseReflectivity) * pow(1.f - HdotV, 5.f);
}

void main() {
    const vec3 N = normalize(rNormal);
    const vec3 V = normalize(viewPos - rPos);

    const vec3 baseReflectivity = mix(vec3(0.04f), rAlbedo, rMetallic);

    vec3 Lo = vec3(0.f);
    // repeat for all light sources
    for (int i = 0; i < 1; i++) {
        const vec3 L = normalize(lightPos - rPos);
        const vec3 H = normalize(V + L);
        const float distance = length(lightPos - rPos);
        const float attenuation = 1.0 / (distance);
        vec3 radiance = lightColor * attenuation;

        const float NdotV = max(dot(N, V), 0.0000001f);
        const float NdotL = max(dot(N, L), 0.0000001f);
        const float HdotV = max(dot(H, V), 0.f);
        const float NdotH = max(dot(N, H), 0.f);

        const float D = DistributionGGX(NdotH, rRoughness);
        const float G = GeometrySmith(NdotV, NdotL, rRoughness);
        const vec3 F = FresnelSchlick(HdotV, baseReflectivity);

        const vec3 specular = (D * G * F) / (4.f * NdotV * NdotL);
        vec3 kD = vec3(1.f) - F;

        kD *= 1.f - rMetallic;

        Lo += (kD * rAlbedo / PI + specular) * radiance * NdotL;
        
    }

    const vec3 ambient = vec3(0.03f) * rAlbedo;

    vec3 color = ambient + Lo;

    // gamma correction
    color = pow(color, vec3(1.f/2.2f));

    FragColor = vec4(color, 1.f);
}

)