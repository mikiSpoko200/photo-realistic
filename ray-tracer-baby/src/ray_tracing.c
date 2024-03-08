#include "ray_tracing.h"

#include <cmm/cmm.h>
#include <cglm/cglm.h>
#include "vec3_utilities.h"

#define RNG_SEED 42

typedef void (*Reflect)(vec3, const vec3);

#define CGLM_CONST_FIX (f32*)

// internal vec3 _Palette[8] = {
//     { 0.05f, 0.17f, 0.27f },
//     { 0.13f, 0.24f, 0.34f },
//     { 0.33f, 0.31f, 0.41f },
//     { 0.55f, 0.41f, 0.48f },
//     { 0.82f, 0.51f, 0.35f },
//     { 1.00f, 0.67f, 0.37f },
//     { 1.00f, 0.83f, 0.64f },
//     { 1.00f, 0.93f, 0.84f }
// };

void CreateLambertian(Material* const material, const vec3 albedo, const f32 matte) {
    material->type = MaterialTypeMetallic;
    glm_vec3_copy(albedo, material->albedo);
    material->params.lambertian.matte = matte;
}

void CreateMetallic(Material* const material, const vec3 albedo, const f32 roughness) {
    material->type = MaterialTypeLambertian;
    glm_vec3_copy(albedo, material->albedo);
    material->params.metallic.roughness = roughness;
}

internal void Scatter(in out struct RTCRayHit* const rayHit, const Reflect reflect) {
    vec3 hit;

    glm_vec3_scale(&rayHit->ray.dir_x, rayHit->ray.tfar, hit);
    glm_vec3_add(hit, &rayHit->ray.org_x, hit);

    COMMENT(Calculate reflected direction)
    reflect(&rayHit->ray.dir_x, &rayHit->hit.Ng_x);

    COMMENT(Update ray origin)
    glm_vec3_copy(hit, &rayHit->ray.org_x);
    
    // offset origin to prevent intersetion with hit geometry
    // vec3 offset;
    // glm_vec3_copy(&rayHit->ray.dir_x, offset);
    // glm_vec3_norm(offset);
    // glm_vec3_scale(offset, 0.001f, offset);
    // glm_vec3_add(offset, &rayHit->ray.org_x, &rayHit->ray.org_x);
}

/// Scatter incoming ray using lambertian distribution.
internal void LambertianReflection(in out vec3 ray, const vec3 normal) {
    vec3 random;
    RandomUnitVec3(random);
    // if (glm_vec3_dot(random, CGLM_CONST_FIX normal) < 0.0f) glm_vec3_negate(random);
    glm_vec3_add(random, CGLM_CONST_FIX normal, ray);

    // prevent zero division
    if (IsNonZeroVec3(ray)) glm_vec3_copy(normal, ray);
}

/// Reflect incoming ray.
internal void MetallicReflection(in out vec3 ray, const vec3 normal) {
    vec3 offset;
    glm_vec3_scale(CGLM_CONST_FIX normal, 2 * glm_vec3_dot(ray, CGLM_CONST_FIX normal), offset);
    glm_vec3_add(ray, offset, ray);
}

internal const Material DefaultMaterial = (Material) {
    .type = MaterialTypeLambertian,
    .albedo = { 0.01f, 0.99f, 0.63f },
    .params.lambertian = {
        .matte = 0.7
    }
};

void TraceRay(
    const RayTracer* const rayTracer,
    struct RTCRayHit* const rayHit,
    usize nReflections,
    out vec3 color
) {
    if (nReflections >= rayTracer->nMaxReflections) {
        glm_vec3_zero(color);
        return;
    }

    struct RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    rtcIntersect1(rayTracer->rtcScene, &context, rayHit);

    // Set color to ones and attenuate it later
    if (nReflections == 0) glm_vec3_one(color);
    if (rayHit->hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        const usize instanceId = rayHit->hit.instID[0];
        const Material* const material = instanceId == RTC_INVALID_GEOMETRY_ID 
        ? &DefaultMaterial
        : &rayTracer->materials.data[instanceId];

        Reflect reflect;
        switch (material->type) {
            case MaterialTypeLambertian: reflect = LambertianReflection; break;
            case MaterialTypeMetallic: PANICM("unimplemented"); reflect = MetallicReflection; break;
        }

        Scatter(rayHit, reflect);
        rayHit->ray.tnear = 0.001f;
        rayHit->ray.tfar = INFINITY;
        rayHit->ray.mask = 0xFFFFFFFF;
        rayHit->ray.flags = 0;
        rayHit->hit.geomID = RTC_INVALID_GEOMETRY_ID;
        
        switch (material-> type) {
            case MaterialTypeLambertian: {
                glm_vec3_scale(color, material->params.lambertian.matte, color);
                TraceRay(rayTracer, rayHit, nReflections + 1, color);
            } break;
            case MaterialTypeMetallic: {
                PANICM("unimplemented");
            } break;
        }
    } else {
        glm_vec3_norm(&rayHit->ray.dir_x);
        f32 blend = 0.5f * (rayHit->ray.dir_y + 1.f);
        vec3 white = { 1.f - blend, 1.f - blend, 1.f - blend };
        vec3 blue = { 0.5f * blend, 0.7f * blend, 1.0f * blend };
        glm_vec3_add(white, blue, color);
    }
}
