#pragma once

#include <cmm/cmm.h>
#include <cglm/cglm.h>

#include "obj.h"

#define GL_ASSERT_NO_ERROR glCheckError_(__FILE__, __func__, __LINE__)

#define INVERTED_NORMALS
#undef INVERTED_NORMALS

typedef struct {
    vec3 albedo;
    f32 matte;
} MaterialRaster;

DeclareArray(MaterialRaster);

typedef struct {
    Position position;
    vec3 color;
} PointLight;

DeclareArray(PointLight);

typedef struct {
    Obj obj;
    usize id;
} Mesh;

/// Data for mesh instancing
typedef struct {
    mat4 model;
    MaterialRaster material;
} Instance;

typedef struct {
    usize nMeshes;
    const char* const * const objPaths;
    const char* vs;
    const char* fs;
} RendererConfig;


DeclareArray(Mesh);
DeclareArray(Instance);

typedef Array(Mesh) Meshes;
typedef Array(Instance) Instances;

typedef void(*RendererInitializer)(RendererConfig);
typedef void(*RendererDropper)(void);

typedef struct {
    usize vertex;
    usize index;
} Offsets;

void RendererInitialize(RendererConfig config);
void RendererDrop(void);
void Render(void);

void ToggleWireFrame(void);

void MoveCamera(vec3 direction, bool relative);
void RotateCamera(f64 pitch, f64 yaw);

u32 CreateProgram(const char* vs, const char* fs);
void DeleteProgram(u32 program);

void CreateInstance(Transform* transform, const MaterialRaster* material, Instance* instance);
void glCheckError_(const char* file, const char* func, int line);

/// Registers new instances of mesh for rendering.
void AddInstances(usize meshId, Array(Instance) instances);

/// @returns pointer to instance data for given instance id
Instance* GetInstance(usize meshId, usize instanceId);

typedef struct {
    vec3 position;
    vec3 direction;
    vec3 up;

    versor rotation;
    f32 speed;

    f32 fov;
    const f32 nearPlane;
    const f32 farPlane; 
} Camera3D;

extern struct Renderer {
    u32 program;
    Array(Mesh) meshes;
    Offsets* byteOffsets;

    Camera3D camera;
    mat4 view;
    mat4 perspective;

    vec3 lightPosition;
    vec3 lightColor;

    u32 meshVbo;
    u32 instanceVbo;
    u32 vao;
    u32 ebo;

    bool addWireFrame;

    const RendererInitializer initialize;
    const RendererDropper drop;
} Renderer;

#ifdef _DEBUG
#define LOGF_CAMERA_STATE() LOGF("Camera at [" FSFA(f32, "+0.5", 3) "] look direction [" FSFA(f32, "+0.5", 3) "]", FSA_UNROLL(Renderer.camera.position, 3), FSA_UNROLL(Renderer.camera.direction, 3));
#define LOGF_LIGHTING_STATE() LOGF("Light at [" FSFA(f32, "+0.5", 3) "] color [" FSFA(f32, "+0.5", 3) "]", FSA_UNROLL(Renderer.lightPosition, 3), FSA_UNROLL(Renderer.lightColor, 3));
#else
#define LOGF_CAMERA_STATE()
#endif