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

typedef u8 Rgb256[3];
DeclareArray(Rgb256);

typedef struct {
    usize width;
    usize height;
    Rgb256 *buffer;
} Buffer2d;

typedef struct {
    RTCScene rtcScene;
    Array(Material) materials;
    usize nRaysPerSample;
    vec3 skyColor;
    usize nMaxReflections;
} RayTracer;

void CreateLambertian(Material* material, const vec3 albedo, f32 matte);

void CreateMetallic(Material* material, const vec3 albedo, f32 roughness);

void TraceRay(const RayTracer* rayTracer, struct RTCRayHit* ray, usize nReflections, vec3 outColor);
