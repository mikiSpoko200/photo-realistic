#include "renderer.h"

#include <GL/glew.h>
#include <cglm/cglm.h>

#include <cmm/cmm.h>
#include <cmm/units.h>

#include "attributes.h"
#include "shaders.h"
#include "obj.h"

#define MAX_INSTANCES (usize)10000
#define MAX_MESHES (usize)100


void glCheckError_(const char* file, const char* func, int line) {
    u32 errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR)
    {
        const char* error;
        switch (errorCode)
        {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
            default: PANIC_WITH_CONTEXT("Unsupported OpenGL error code:" FS(u32), file, func, line, errorCode);
        }
        PANIC_WITH_CONTEXT("%s", file, func, line, error);
    }
}

internal Instance InstanceStore[MAX_MESHES][MAX_INSTANCES];
internal usize InstanceStoreLen[MAX_MESHES];

#define N_INSTANCES(meshId) InstanceStoreLen[meshId]
#define INSTANCE_OFFSET(meshId) (N_INSTANCES(meshId) * sizeof* InstanceStore[meshId])

struct Renderer Renderer = {
        .program = 0,
        .byteOffsets = NULL,
        .camera = {
            .fov = 60.f,
            .nearPlane = 0.1f,
            .farPlane = 1000.f
        },
        .meshVbo = 0,
        .instanceVbo = 0,
        .vao = 0,
        .ebo = 0,
        .initialize = RendererInitialize,
        .drop = RendererDrop,
};

u32 CreateProgram(const char* const vs, const char* const fs) {
    i32 compiled, linked = 0;

    char* errorLog = (char*)calloc(256, sizeof(char));

    const u32 v = glCreateShader(GL_VERTEX_SHADER);
    LOGLN("Created vertex shader:" FS(u32), v);

    glShaderSource(v, 1, &vs, NULL);
    glCompileShader(v);
    GL_ASSERT_NO_ERROR;

    // check if shader compiled
    glGetShaderiv(v, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        i32 logLength = 0;
        glGetShaderiv(v, GL_INFO_LOG_LENGTH, &logLength);
        GL_ASSERT_NO_ERROR;
        errorLog = (char*)realloc(errorLog, logLength);

        glGetShaderInfoLog(v, logLength, NULL, errorLog);
        LOGLN("Vertex shader compilation failed:\n%s", errorLog);
        glDeleteShader(v);
        free(errorLog);
        return 0;
    }

    LOGLNM("Vertex shader compilation successful");

    const u32 f = glCreateShader(GL_FRAGMENT_SHADER);
    LOGLN("Created fragment shader:" FS(u32), f);

    glShaderSource(f, 1, &fs, NULL);
    glCompileShader(f);

    // check if shader compiled
    glGetShaderiv(f, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        i32 logLength = 0;
        glGetShaderiv(f, GL_INFO_LOG_LENGTH, &logLength);
        errorLog = (char*)realloc(errorLog, sizeof(char));

        glGetShaderInfoLog(f, logLength, NULL, errorLog);
        PANIC("Fragment shader compilation failed:\n%s", errorLog);
        glDeleteShader(f);
        free(errorLog);
        return 0;
    }
    LOGLNM("Fragment shader compilation successful");

    const u32 p = glCreateProgram();

    glAttachShader(p, v);
    glAttachShader(p, f);

    glLinkProgram(p);

    glDetachShader(p, v);
    glDetachShader(p, f);

    glGetProgramiv(p, GL_LINK_STATUS, &linked);

    if (!linked) {
        int logLength = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &logLength);
        errorLog = (char*)realloc(errorLog, logLength);

        glGetProgramInfoLog(p, logLength, NULL, errorLog);
        glDeleteProgram(p);
        PRINTLN("Shader compilation error: %s", errorLog);
        free(errorLog);
        return 0;
    }
    LOGLNM("Program linking successful");

    glDeleteShader(v);
    glDeleteShader(f);
    free(errorLog);

    return p;
}

void DeleteProgram(const u32 program) {
    glDeleteProgram(program);
}

internal void UpdateViewMatrix(void) {
    vec3 center; // Center point of the view
    glm_vec3_add(Renderer.camera.position, Renderer.camera.direction, center);
    glm_lookat(Renderer.camera.position, center, (vec3) { 0.f, 1.f, 0.f }, Renderer.view);

    glUniform3f(VIEW_POSITION_LOCATION,  FSA_UNROLL(Renderer.camera.position, 3)); GL_ASSERT_NO_ERROR;
    glUniformMatrix4fv(VIEW_MATRIX_LOCATION      , 1, GL_FALSE, (f32*)Renderer.view); GL_ASSERT_NO_ERROR;
}

internal void UpdatePerspectiveMatrix(void) {
    glm_perspective(
        glm_rad(Renderer.camera.fov),
        4.0f / 3.0f,
        Renderer.camera.nearPlane,
        Renderer.camera.farPlane,
        Renderer.perspective
    );
}

internal void InitializeCamera(void) {
    glm_vec3_copy((vec3) { 6.f, 6.f, 6.f }, Renderer.camera.position);
    glm_vec3_copy((vec3) { 0.f, 0.f, 0.f }, Renderer.camera.direction);
    glm_vec3_copy((vec3) { 0.f, 1.f, 0.f }, Renderer.camera.up);
    glm_quat_identity(Renderer.camera.rotation);
    UpdateViewMatrix();
    UpdatePerspectiveMatrix();
}

internal void InitializeLighting(void) {
    // 5.f, 5.f, 5.f
    glm_vec3_copy((vec3) { 6.f, 6.f, 6.f }, Renderer.lightPosition);
    glm_vec3_copy((vec3) { 1.f, 1.f, 1.f }, Renderer.lightColor);
}

#define VERTEX_BYTE_OFFSET(i) Renderer.byteOffsets[i].vertex
#define INDEX_BYTE_OFFSET(i) Renderer.byteOffsets[i].index
#define OBJ(i) Renderer.meshes.data[i].obj

#define VERTEX_BYTE_SIZE(i) (OBJ(i).nVertices * sizeof(Vertex))
#define INDEX_BYTE_SIZE(i) (OBJ(i).nIndices * sizeof(u16))

void RendererInitialize(const RendererConfig config) {
    LOGLNM("Initializing renderer");
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    LOGLN("Setting clear color to RGBA:" FSA(f32, 4), 0.1255f, 0.2039f, 0.2275f, 1.0f);
    glClearColor(0.1255f, 0.2039f, 0.2275f, 1.0f);

    LOGLNM("Compiling shaders");
    if ((Renderer.program = CreateProgram(config.vs, config.fs)) == 0) {
        PANICM("Program creation failed");
    }

    glUseProgram(Renderer.program);

    LOGLNM("Initializing Camera");
    InitializeCamera();
    LOGLNM("Initializing Lighting");
    InitializeLighting();

    Renderer.meshes = AllocateArray(Mesh, config.nMeshes);
    Renderer.byteOffsets = (Offsets*)malloc(Renderer.meshes.len * sizeof(Offsets));

    usize totalIndexSize = 0;
    usize totalVertexSize = 0;
    VERTEX_BYTE_OFFSET(0) = 0;
    INDEX_BYTE_OFFSET(0) = 0;
    LOGLN("Loading" FS(usize) ".obj models", Renderer.meshes.len);
    for (usize i = 0; i < Renderer.meshes.len; i++) {
        Renderer.meshes.data[i].id = i;
        LoadOBJ(config.objPaths[i], &OBJ(i));
        totalIndexSize += OBJ(i).nIndices * sizeof(u16);
        totalVertexSize += OBJ(i).nVertices * sizeof(Vertex);

        if (i > 0) {
            VERTEX_BYTE_OFFSET(i) =
                VERTEX_BYTE_OFFSET(i - 1) // offset by however much the previous object was offset
                + VERTEX_BYTE_SIZE(i - 1); // and offset by the previous' object data.
            INDEX_BYTE_OFFSET(i) = INDEX_BYTE_OFFSET(i - 1) + INDEX_BYTE_SIZE(i - 1);
        }
    }

    glCreateBuffers(1, &Renderer.meshVbo);
    glCreateBuffers(1, &Renderer.instanceVbo);
    glCreateBuffers(1, &Renderer.ebo);
    glCreateVertexArrays(1, &Renderer.vao);

    glBindVertexArray(Renderer.vao);

    const usize last = Renderer.meshes.len - 1;

    ASSERT_EQ(totalVertexSize, VERTEX_BYTE_OFFSET(last) + VERTEX_BYTE_SIZE(last));
    ASSERT_EQ(totalIndexSize, INDEX_BYTE_OFFSET(last) + INDEX_BYTE_SIZE(last));

    // Allocate instance buffer
    LOGLN("Allocating" FS(usize) "MB for mesh vertices", MB(MAX_INSTANCES * MAX_MESHES * sizeof(Instance)));
    glBindBuffer(GL_ARRAY_BUFFER, Renderer.instanceVbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCES * MAX_MESHES * sizeof(Instance), NULL, GL_DYNAMIC_DRAW);

    // Allocate model vertex buffer
    LOGLN("Allocating" FS(usize) "bytes for mesh vertices", totalVertexSize);
    glBindBuffer(GL_ARRAY_BUFFER, Renderer.meshVbo);
    glBufferData(GL_ARRAY_BUFFER, totalVertexSize, NULL, GL_STATIC_DRAW);
    // Allocate model index buffer
    LOGLN("Allocating" FS(usize) "bytes for mesh indexes", totalIndexSize);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Renderer.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, totalIndexSize, NULL, GL_STATIC_DRAW);

    LOGLNM("Uploading meshes to GPU");
    for (usize i = 0; i < Renderer.meshes.len; i++) {
        LOGLN("  Mesh" FS(usize) "--" FS(usize) "bytes", i, VERTEX_BYTE_SIZE(i));
        glBufferSubData(GL_ARRAY_BUFFER, VERTEX_BYTE_OFFSET(i), VERTEX_BYTE_SIZE(i), OBJ(i).vertices);
        GL_ASSERT_NO_ERROR;
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, INDEX_BYTE_OFFSET(i), INDEX_BYTE_SIZE(i), OBJ(i).indices);
        GL_ASSERT_NO_ERROR;
    }

    // Configure vertex array pointers

    // Configure meshes
    LOGLNM("Configuring mesh attributes");
    glVertexAttribPointer(POSITION_LOCATION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(POSITION_LOCATION);
    LOGLN ("  Vertex::Location   :" FS(u32), POSITION_LOCATION);
    LOGLN ("  Vertex::Size       :" FS(i32), 3);
    LOGLN ("  Vertex::Type       : %s" , STRINGIFY(GL_FLOAT));
    LOGLN ("  Vertex::Normalized : %s" , STRINGIFY(GL_FALSE));
    LOGLN ("  Vertex::Stride     :" FS(usize), sizeof(Vertex));
    LOGLN ("  Vertex::Offset     :" FS(u32), 0);

    glVertexAttribPointer(NORMAL_INDEX, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)sizeof(Position));  // NOLINT(performance-no-int-to-ptr)
    glEnableVertexAttribArray(NORMAL_INDEX);
    LOGLN ("  Normal::Location   :" FS(u32), NORMAL_INDEX);
    LOGLN ("  Normal::Size       :" FS(i32), 3);
    LOGLN ("  Normal::Type       : %s" , STRINGIFY(GL_FLOAT));
    LOGLN ("  Normal::Normalized : %s" , STRINGIFY(GL_FALSE));
    LOGLN ("  Normal::Stride     :" FS(usize), sizeof(Vertex));
    LOGLN ("  Normal::Offset     :" FS(usize), sizeof(Position));

    // Configure instance parameters
    glBindBuffer(GL_ARRAY_BUFFER, Renderer.instanceVbo);
    glVertexAttribPointer(MODEL_LOCATION + 0, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)0);
    glVertexAttribPointer(MODEL_LOCATION + 1, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(sizeof(f32) * 4));
    glVertexAttribPointer(MODEL_LOCATION + 2, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(sizeof(f32) * 8));
    glVertexAttribPointer(MODEL_LOCATION + 3, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(sizeof(f32) * 12));
    glVertexAttribPointer(ALBEDO_LOCATION   , 3, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(sizeof(f32) * 16));
    glVertexAttribPointer( ROUGHNESS_LOCATION, 1, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(sizeof(f32) * 19));
    glVertexAttribPointer(FRESNEL_FACTOR__LOCATION, 1, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(sizeof(f32) * 20));
    glEnableVertexAttribArray(MODEL_LOCATION + 0);
    glEnableVertexAttribArray(MODEL_LOCATION + 1);
    glEnableVertexAttribArray(MODEL_LOCATION + 2);
    glEnableVertexAttribArray(MODEL_LOCATION + 3);
    glEnableVertexAttribArray(ALBEDO_LOCATION);
    glEnableVertexAttribArray( ROUGHNESS_LOCATION);
    glEnableVertexAttribArray(FRESNEL_FACTOR__LOCATION);
    glVertexAttribDivisor(MODEL_LOCATION + 0, 1);
    glVertexAttribDivisor(MODEL_LOCATION + 1, 1);
    glVertexAttribDivisor(MODEL_LOCATION + 2, 1);
    glVertexAttribDivisor(MODEL_LOCATION + 3, 1);
    glVertexAttribDivisor(MODEL_LOCATION + 0, 1);
    glVertexAttribDivisor(ALBEDO_LOCATION, 1);
    glVertexAttribDivisor(ROUGHNESS_LOCATION, 1);
    glVertexAttribDivisor(FRESNEL_FACTOR__LOCATION, 1);

#ifdef INVERTED_NORMALS
    glVertexAttribPointer(TRANSPOSE_INVERSE_MODEL_LOCATION + 0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4 * 4, (void*)0);
    glVertexAttribPointer(TRANSPOSE_INVERSE_MODEL_LOCATION + 1, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4 * 4, (void*)(sizeof(float) * 4));
    glVertexAttribPointer(TRANSPOSE_INVERSE_MODEL_LOCATION + 2, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4 * 4, (void*)(sizeof(float) * 8));
    glVertexAttribPointer(TRANSPOSE_INVERSE_MODEL_LOCATION + 3, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4 * 4, (void*)(sizeof(float) * 12));
    glEnableVertexAttribArray(TRANSPOSE_INVERSE_MODEL_LOCATION + 0);
    glEnableVertexAttribArray(TRANSPOSE_INVERSE_MODEL_LOCATION + 1);
    glEnableVertexAttribArray(TRANSPOSE_INVERSE_MODEL_LOCATION + 2);
    glEnableVertexAttribArray(TRANSPOSE_INVERSE_MODEL_LOCATION + 3);
    glVertexAttribDivisor(TRANSPOSE_INVERSE_MODEL_LOCATION + 0, 1);
    glVertexAttribDivisor(TRANSPOSE_INVERSE_MODEL_LOCATION + 1, 1);
    glVertexAttribDivisor(TRANSPOSE_INVERSE_MODEL_LOCATION + 2, 1);
    glVertexAttribDivisor(TRANSPOSE_INVERSE_MODEL_LOCATION + 3, 1);
#endif

    LOGLNM("Enabling depth test");
    glEnable(GL_DEPTH_TEST);
    LOGLNM("Enabling face culling");
    glEnable(GL_CULL_FACE);

    LOGLNM("Loading uniforms");
    glUniformMatrix4fv(VIEW_MATRIX_LOCATION      , 1, GL_FALSE, (f32*)Renderer.view);
    glUniformMatrix4fv(PROJECTION_MATRIX_LOCATION, 1, GL_FALSE, (f32*)Renderer.perspective);
    glUniform3f(VIEW_POSITION_LOCATION,  FSA_UNROLL(Renderer.camera.position, 3));
    glUniform3f(LIGHT_POSITION_LOCATION, FSA_UNROLL(Renderer.lightPosition, 3));
    glUniform3f(LIGHT_COLOR_LOCATION,    FSA_UNROLL(Renderer.lightColor, 3));

    const f32 lineWidth = 1.f;
    LOGLN("Setting wire-frame line width to" FS(f32), lineWidth);
    glLineWidth(lineWidth); GL_ASSERT_NO_ERROR;
    f32 width;
    glGetFloatv(GL_LINE_WIDTH, &width);
    LOGLN("Checking line width:" FS(f32), width);
}

void Render(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (usize meshId = 0; meshId < Renderer.meshes.len; meshId++) {
        GL_ASSERT_NO_ERROR;
        glDrawElementsInstanced(GL_TRIANGLES, (i32)OBJ(meshId).nIndices, GL_UNSIGNED_SHORT, (const void*)INDEX_BYTE_OFFSET(meshId), N_INSTANCES(meshId));
        if (Renderer.addWireFrame) {
            glPolygonMode(GL_FRONT_AND_BACK , GL_LINE); GL_ASSERT_NO_ERROR;
            glDrawElementsInstanced(GL_TRIANGLES, (i32)OBJ(meshId).nIndices, GL_UNSIGNED_SHORT, (const void*)INDEX_BYTE_OFFSET(meshId), N_INSTANCES(meshId));
            glPolygonMode(GL_FRONT_AND_BACK , GL_FILL); GL_ASSERT_NO_ERROR;
        }
        GL_ASSERT_NO_ERROR;
	}
}

void ToggleWireFrame(void) {
    Renderer.addWireFrame = !Renderer.addWireFrame;
}

#undef VERTEX_BYTE_OFFSET
#undef INDEX_BYTE_OFFSET
#undef VERTEX_BYTE_SIZE
#undef INDEX_BYTE_SIZE

void RendererDrop(void) {
    LOGLNM("Dropping renderer");

    LOGLN("Freeing" FS(usize) ".obj models", Renderer.meshes.len);
    for (usize i = 0; i < Renderer.meshes.len; i++) {
        FreeOBJ(OBJ(i));
    }
    FreeArray(Renderer.meshes);
    free(Renderer.byteOffsets);
}

void CreateInstance(Transform* const transform, const MaterialRaster* const material, Instance* const Instance) {
    mat4 translationMatrix;
    glm_translate_make(translationMatrix, transform->translation);

    mat4 rotationMatrix;
    glm_rotate_make(rotationMatrix, transform->rotation[0], (vec3){ 1.0f, 0.0f, 0.0f });
    glm_rotate(rotationMatrix, transform->rotation[1], (vec3){ 0.0f, 1.0f, 0.0f });
    glm_rotate(rotationMatrix, transform->rotation[2], (vec3){ 0.0f, 0.0f, 1.0f });

    mat4 scaleMatrix;
    glm_scale_make(scaleMatrix, transform->scale);
    glm_mat4_mul(scaleMatrix, rotationMatrix, rotationMatrix);
    glm_mat4_mul(rotationMatrix, translationMatrix, translationMatrix);

    glm_mat4_copy(translationMatrix, Instance->model);
    Instance->material = *material;
}

void AddInstances(const usize meshId, const Instances instances) {
    const usize nInstances = N_INSTANCES(meshId);
    if (nInstances + instances.len > MAX_INSTANCES) {
        PANIC("Max instance count for mesh" FS(usize)
              "exceeded with push of" FS(usize)
              "instances (max" FS(usize)
              ", overflow: " FS(usize)
              ")", meshId, instances.len, MAX_INSTANCES, nInstances + instances.len - MAX_INSTANCES);
    }
    // Update CPU memory
    memcpy(&InstanceStore[meshId][nInstances], instances.data, instances.len * sizeof(Instance));

    // Update GPU memory
    glBindBuffer(GL_ARRAY_BUFFER, Renderer.instanceVbo);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        INSTANCE_OFFSET(meshId),
        instances.len * sizeof(Instance),
        instances.data);
    InstanceStoreLen[meshId] += instances.len;
}

Instance* GetInstance(const usize meshId, const usize instanceId) {
    return &InstanceStore[meshId][instanceId];
}

#undef OBJ

void MoveCamera(vec3 direction, const bool relative) {
    direction[0] *= Renderer.camera.speed;
    direction[1] *= Renderer.camera.speed;
    direction[2] *= Renderer.camera.speed;

    if (relative) {
        glm_quat_rotatev(Renderer.camera.rotation, direction, direction);
    }
    glm_vec3_add(Renderer.camera.position, direction, Renderer.camera.position);
    UpdateViewMatrix();
}

#define PITCH_SENSITIVITY 2.f
#define YAW_SENSITIVITY 2.f

#define _USE_MATH_DEFINES
#include "math.h"

void RotateCamera(const f64 pitch, const f64 yaw) {
    // LOGLN("pitch" FS(f32) "yaw" FS(f32), pitch * PITCH_SENSITIVITY / 360 * M_2_PI, yaw   * YAW_SENSITIVITY / 360 * M_2_PI);
    versor pitchQuat, yawQuat;
    glm_quat(pitchQuat, pitch * PITCH_SENSITIVITY / 360.f * M_2_PI, 1.f, 0.f, 0.f);
    glm_quat(yawQuat  , yaw   * YAW_SENSITIVITY   / 360.f * M_2_PI, 0.f, 1.f, 0.f);

    // Update Camera Rotation quaternion (NOTE: maybe incorrect)
    glm_quat_mul(Renderer.camera.rotation, pitchQuat, Renderer.camera.rotation);
    glm_quat_mul(Renderer.camera.rotation, yawQuat  , Renderer.camera.rotation);

    // Update the camera's direction and up vectors based on the new rotation
    mat4 rotationMatrix;
    glm_quat_mat4(Renderer.camera.rotation, rotationMatrix);
    glm_mat4_mulv3(rotationMatrix, (vec3){ 0.f, 0.f, -1.f }, 1.f, Renderer.camera.direction);
    glm_mat4_mulv3(rotationMatrix, (vec3){ 0.f, 1.f,  0.f }, 1.f, Renderer.camera.up);
    UpdateViewMatrix();
}

