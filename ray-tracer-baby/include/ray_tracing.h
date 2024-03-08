#pragma once

#include <cmm/cmm.h>
#include <cglm/cglm.h>
#include <embree3/rtcore.h>

#include "renderer.h"
#include "attributes.h"

enum MaterialType {
    MaterialTypeLambertian,
    MaterialTypeMetallic,
};

typedef struct {
    f32 matte;
} LambertianMaterial;

typedef struct {
    f32 roughness;
} MetallicMaterial;

typedef struct {
    enum MaterialType type;
    vec3 albedo;
    union {
        LambertianMaterial lambertian;
        MetallicMaterial metallic;
    } params;
} Material;

DeclareArray(Material);

void CreateLambertian(Material* material, const vec3 albedo, f32 matte);

void CreateMetallic(Material* material, const vec3 albedo, f32 roughness);

typedef struct {
    RTCScene rtcScene;
    Array(Material) materials;

    vec3 skyColor;
    usize nMaxReflections;
} RayTracer;

void TraceRay(const RayTracer* rayTracer, struct RTCRayHit* ray, usize nReflections, vec3 outColor);
