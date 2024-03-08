#pragma once

#include <cmm/cmm.h>
#include <cglm/cglm.h>

// Typedef for 3D vertex position
typedef struct {
    f32 x, y, z;
} Position;

 // Typedef for 2D texture coordinates
 typedef struct {
     f32 u, v;
 } TexCoord;

// Typedef for 3D normals
typedef struct {
    f32 nx, ny, nz;
} Normal;


typedef struct {
    f32 radius;
    vec3 position;
} Sphere;

typedef struct {
    vec3 translation;
    vec3 rotation;
    vec3 scale;
} Transform;

typedef struct {
    Sphere boundingSphere;
    u32 Id;
} ObjectData;
