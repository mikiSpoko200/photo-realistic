STRINGIFY(

// Mesh parameters
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
// Instance parameters
layout(location = 2) in mat4 iModel;
layout(location = 6) in vec3 iAlbedo;
layout(location = 7) in float iRoughness;
layout(location = 8) in float iMetallic;
// layout(location = 9) in mat4 iInverseModel;

out vec3 rPos;
out vec3 rNormal;
flat out vec3 rAlbedo;
flat out float rRoughness;
flat out float rMetallic;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 projection;

void main() {
    // pass parameters to fragment shader
    const mat4 view_model = view * iModel;

    rNormal = transpose(inverse(mat3(view_model))) * aNormal;
    rAlbedo = iAlbedo;
    rRoughness = iRoughness;
    rMetallic = iMetallic;
    // transform vertex positions into world space
    rPos = vec3(iModel * vec4(aPos, 1.0));
    gl_Position = projection * view_model * vec4(aPos, 1.0);
}

)
