#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>

#include <embree3/rtcore.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include <stb/stb_image_write.h>
#include <cmm/cmm.h>

#include "renderer.h"
#include "shaders.h"
#include "ray_tracing.h"


DeclareArray(vec3);

#define RNG_SEED 42

#define N_INSTANCES 50
#define X 0
#define Y 1
#define Z 2

internal struct Config {
    int glVersionMajor;
    int glVersionMinor;
} Config = {
    .glVersionMajor = 4,
    .glVersionMinor = 2
};


internal void ErrorCallback(int _, const char* const description) {
    const isize result = fputs(description, stderr);
    if (result == EOF) {
        printf("Failed to write error description to stderr\n");
    }
}

#if defined(RTC_NAMESPACE_USE)
RTC_NAMESPACE_USE
#endif

void EmbreeErrorCallback(void* const _, enum RTCError error, const char* const str) {
    PANIC("embree error ::" FS(i32) ":: %s", error, str);
}

internal struct AppState {
    const usize width;
    const usize height;
    const char* const title;
    bool glInitialized;
    struct {
        bool forward;
        bool backward;
        bool left;
        bool right;
        bool up;
        bool down;
        bool wireFrame;
        bool leftCtrl;
    } keyStates;
    bool cursorDisabled;
    GLFWwindow* window;
    f64 xPos;
    f64 yPos;
} AppState = {
    .width = 800,
    .height = 600,
    .title = "ray-tracer-baby",
    .glInitialized = false,
    .cursorDisabled = false, // toggle it during initialization
};

internal f32 Palette1[8][3] = {
    { 0.05f, 0.17f, 0.27f },
    { 0.13f, 0.24f, 0.34f },
    { 0.33f, 0.31f, 0.41f },
    { 0.55f, 0.41f, 0.48f },
    { 0.82f, 0.51f, 0.35f },
    { 1.00f, 0.67f, 0.37f },
    { 1.00f, 0.83f, 0.64f },
    { 1.00f, 0.93f, 0.84f },
};

internal void HandleMouse(GLFWwindow* _, const f64 xPos, const f64 yPos) {
    RotateCamera(AppState.yPos - yPos, AppState.xPos - xPos);
    AppState.xPos = xPos;
    AppState.yPos = yPos;
}

internal void ToggleCursorMode(void) {
    AppState.cursorDisabled = !AppState.cursorDisabled;
    LOGLN("Cursor disables: " FS(u8), (u8)AppState.cursorDisabled);
    glfwSetInputMode(AppState.window, GLFW_CURSOR, AppState.cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

#define ACTIVATE_ON_PRESS(member, action) do { \
    if ((action) == GLFW_PRESS) \
        AppState.keyStates.member = true; \
    else if ((action) == GLFW_RELEASE) \
        AppState.keyStates.member = false; \
} while(0)


internal void HandleKey(GLFWwindow* window, const i32 key, const i32 scancode, const i32 action, int _) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    } else {
        switch (key) {
            case GLFW_KEY_T: {
                if (action == GLFW_PRESS) ToggleCursorMode();
            } break;
            case GLFW_KEY_W: {
                ACTIVATE_ON_PRESS(forward, action);
            } break;
            case GLFW_KEY_S: {
                ACTIVATE_ON_PRESS(backward, action);
            } break;
            case GLFW_KEY_A: {
                ACTIVATE_ON_PRESS(left, action);
            } break;
            case GLFW_KEY_D: {
                ACTIVATE_ON_PRESS(right, action);
            } break;
            case GLFW_KEY_Q: {
                ACTIVATE_ON_PRESS(down, action);
            } break;
            case GLFW_KEY_E: {
                ACTIVATE_ON_PRESS(up, action);
            } break;
            case GLFW_KEY_LEFT_CONTROL: {
                ACTIVATE_ON_PRESS(leftCtrl, action);
            } break;
            case GLFW_KEY_F: {
                if (AppState.glInitialized && action == GLFW_PRESS) {
                    ToggleWireFrame();
                }
            } break;
            default:
                NOOP;
        }
    }
}

#define MOVE_SPEED 0.01f

#undef ACTIVATE_ON_PRESS

// ReSharper disable once CppInconsistentNaming
#define M_forward  ((vec3) { 0.0f, 0.0f, -MOVE_SPEED })
// ReSharper disable once CppInconsistentNaming
#define M_backward ((vec3) { 0.0f, 0.0f, MOVE_SPEED })
// ReSharper disable once CppInconsistentNaming
#define M_left     ((vec3) { -MOVE_SPEED, 0.0f, 0.0f })
// ReSharper disable once CppInconsistentNaming
#define M_right    ((vec3) { MOVE_SPEED, 0.0f, 0.0f })
// ReSharper disable once CppInconsistentNaming
#define M_up       ((vec3) { 0.0f, MOVE_SPEED, 0.0f })
// ReSharper disable once CppInconsistentNaming
#define M_down     ((vec3) { 0.0f, -MOVE_SPEED, 0.0f })
#define PROCESS_INPUT(direction, relative) do { if (AppState.keyStates.direction) MoveCamera(M_ ## direction, relative); } while (0)

void CheckOpenGLError(const char* stmt, const char* fname, int line)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("OpenGL error %08x, at %s:%i - for %s\n", err, fname, line, stmt);
        abort();
    }
}

#ifdef _DEBUG
    #define GL_CHECK(stmt) do { \
            stmt; \
            CheckOpenGLError(#stmt, __FILE__, __LINE__); \
        } while (0)
#else
    #define GL_CHECK(stmt) stmt
#endif

internal void ProcessInput(void) {
    PROCESS_INPUT(forward, true);
    PROCESS_INPUT(backward, true);
    PROCESS_INPUT(left, true);
    PROCESS_INPUT(right, true);
    PROCESS_INPUT(up, false);
    PROCESS_INPUT(down, false);
    Renderer.camera.speed = AppState.keyStates.leftCtrl ? 0.2f : 1.f;
}

#undef M_forward
#undef M_backward
#undef M_left
#undef M_right
#undef M_up
#undef M_down
#undef PROCESS_INPUT

DeclareArray(RTCGeometry);

int main(void) {
    glfwSetErrorCallback(ErrorCallback);

    if (!glfwInit()) {
        fprintf(stderr, "GLFW initialization failed\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, Config.glVersionMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, Config.glVersionMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

    GLFWwindow* const window = glfwCreateWindow((i32)AppState.width, (i32)AppState.height, AppState.title, NULL, NULL);
    if (window == NULL) {
        fprintf(stdout, "GLFW window creation failed\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    AppState.window = window;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwMakeContextCurrent(window);

    const GLenum err = glewInit();
    if (GLEW_OK != err) PANIC("Error: %s\n", glewGetErrorString(err));
    PRINTLN("Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));

    AppState.glInitialized = true;

    PRINTLN("Initialized OpenGL context: %s core", (const char*) glGetString(GL_VERSION));

    const char* const objPaths[] = { "../scenes/sphere.obj" };

    const RendererConfig config = {
        .nMeshes = ARRAY_LENGTH(objPaths),
        .objPaths = objPaths,
        .vs = vs,
        .fs = fs
    };
    Renderer.initialize(config);

    glfwSetKeyCallback(window, HandleKey);
    glfwSetCursorPosCallback(window, HandleMouse);
    ToggleCursorMode();

    glfwGetCursorPos(window, &AppState.xPos, &AppState.yPos);
    glfwSwapInterval(0);
    usize frameCount = 0;
    f64 lastUpdate = 0.0;


    COMMENT(--------========[ Create Instances ]========--------)

    const usize nInstances = 25;

    Transform transform = {};
    MaterialRaster material;
    const Instances instances = AllocateArray(Instance, nInstances);
    for (usize i = 0; i < nInstances; i++) {
        const f32 ratio = (f32)(i + 1) / ((f32)nInstances + 1.f);
        glm_vec3_copy((vec3) { ratio, 1.f - ratio, (f32)fabs(0.5f - ratio) }, material.albedo);

        const usize xi = i % 5 + 5;
        const usize yi = (i / 5) % 5 + 5;
        const usize zi = 5;
        
        glm_vec3_copy((vec3) { (f32)xi, (f32)yi, (f32)zi }, transform.translation);
        glm_vec3_copy((vec3) { 0.5f, 0.5f, 0.5f }, transform.scale);
        glm_vec3_copy((vec3) { 0.f, 0.f, 0.f }, transform.rotation);
        CreateInstance(
            &transform,
            &material,
            &instances.data[i]
        );
    }
    AddInstances(0, instances);

    // Process embree geometry
    COMMENT(Renderer.meshes.data[0].obj)

    const RTCDevice device = rtcNewDevice(NULL);
    if (!device) PANIC("error %d: cannot create device", rtcGetDeviceError(NULL));
    rtcSetDeviceErrorFunction(device, EmbreeErrorCallback, NULL);

    RTCScene scene = rtcNewScene(device);
    RTCGeometry mesh = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

    LOGLNM("Allocating embree positions");
    Position* const positions = (Position*)rtcSetNewGeometryBuffer(
        mesh,
        RTC_BUFFER_TYPE_VERTEX, 
        0,
        RTC_FORMAT_FLOAT3, 
        sizeof(Position), 
        Renderer.meshes.data[0].obj.nVertices
    );
    
    ASSERT_EQ(Renderer.meshes.data[0].obj.nIndices % 3, 0);

    LOGLNM("Allocating embree indices");
    u32(*indices)[3] = (u32(*)[3])rtcSetNewGeometryBuffer(
        mesh,
        RTC_BUFFER_TYPE_INDEX,
        0,
        RTC_FORMAT_UINT3,
        3 * sizeof(u32),
        Renderer.meshes.data[0].obj.nIndices / 3
    );

    for (usize i = 0; i < Renderer.meshes.data[0].obj.nVertices; i++) {
        positions[i] = Renderer.meshes.data[0].obj.vertices[i].position;
    }

    for (usize i = 0; i < Renderer.meshes.data[0].obj.nIndices; i++) {
        indices[i / 3][0] = Renderer.meshes.data[0].obj.indices[i + 0];
        indices[i / 3][1] = Renderer.meshes.data[0].obj.indices[i + 1];
        indices[i / 3][2] = Renderer.meshes.data[0].obj.indices[i + 2];
    }

    rtcCommitGeometry(mesh);

    // Attach the geometry to the scene
    rtcAttachGeometry(scene, mesh);
    rtcReleaseGeometry(mesh);

    // Commit the scene
    rtcCommitScene(scene);

    Array(RTCGeometry) embreeInstances = AllocateArray(RTCGeometry, instances.len);
    
    // Create Embree instnaces
    for (usize i = 0; i < instances.len; i++) {
        embreeInstances.data[i] = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(embreeInstances.data[i], scene);
        rtcSetGeometryTransform(embreeInstances.data[i], 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, instances.data[i].model);
    }

    COMMENT(---------===========[ Trace Rays ]===========---------)

    RayTracer rayTracer = (RayTracer) {
        .materials = AllocateArray(Material, instances.len),
        .nMaxReflections = 30,
        .rtcScene = scene,
        .skyColor = { 0.5f, 0.7f, 1.0f },
    };

    for (usize i = 0; i < instances.len; i++) {
        CreateLambertian(&rayTracer.materials.data[i], Palette1[i % ARRAY_LENGTH(Palette1)], (f32)i / instances.len);
    }

    Array(vec3) image = AllocateArray(vec3, AppState.width * AppState.height);

    vec3 lookDir;
    glm_vec3_copy(Renderer.camera.direction, lookDir);

    u8 (*converted)[3] = malloc(AppState.width * AppState.height * 3);
    for (int y = 0; y < AppState.height; ++y) {
        LOGF("%.2f%%", ((f32)y / AppState.height));
        for (int x = 0; x < AppState.width; ++x) {
            struct RTCRayHit rayhit;
            rayhit.ray.org_x = 0.0f;
            rayhit.ray.org_y = 0.0f;
            rayhit.ray.org_z = -1.0f;

            rayhit.ray.dir_x = (2.0f * ((f32)x / AppState.width)) - 1.0f;
            rayhit.ray.dir_y = 1.0f - (2.0f * ((f32)y / AppState.height));
            rayhit.ray.dir_z = 1.0f;
            rayhit.ray.tnear = 0.001f;
            rayhit.ray.tfar = INFINITY;
            rayhit.ray.mask = 0xFFFFFFFF;
            rayhit.ray.flags = 0;
            rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

            glm_vec3_one(image.data[y * AppState.width + x]);
            TraceRay(&rayTracer, &rayhit, 0, image.data[y * AppState.width + x]);
            converted[y * AppState.width + x][X] = (u8)(image.data[y * AppState.width + x][X] * 255.999f);
            converted[y * AppState.width + x][Y] = (u8)(image.data[y * AppState.width + x][Y] * 255.999f);
            converted[y * AppState.width + x][Z] = (u8)(image.data[y * AppState.width + x][Z] * 255.999f);
        }
    }
    PRINTLNM("");
    LOGLNM("Tracing done");

    i32 result = stbi_write_bmp("./test.bmp", AppState.width, AppState.height, 3, converted);
    if (result == 0) PANICM("image failed");
    else LOGLNM("Image written to file");

    u32 texture;
    glGenTextures(1, &texture); GL_ASSERT_NO_ERROR;
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, AppState.width, AppState.height, 0, GL_RGB, GL_UNSIGNED_BYTE, converted); GL_ASSERT_NO_ERROR;

    u32 vao;
    glGenVertexArrays(1, &vao); GL_ASSERT_NO_ERROR;
    glBindVertexArray(vao);

    const char* const vs = SHADER(
        out vec2 texcoords;
        out vec4 color;

        void main() {
            vec2 vertices[3]=vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1, 3));
            gl_Position = vec4(vertices[gl_VertexID], 0, 1);
            color = gl_Position;
            texcoords = 0.5 * gl_Position.xy + vec2(0.5);
        }
    );

    const char* const fs = SHADER(
        out vec4 FragColor;
        
        in vec4 color;
        in vec2 texcoords;

        uniform sampler2D ourTexture;

        void main() {
            FragColor = texture(ourTexture, texcoords) * color;
        }
    );

    const u32 program = CreateProgram(vs, fs);
    glUseProgram(program); GL_ASSERT_NO_ERROR;

    while(1) { 
        glDrawArrays(GL_TRIANGLES, 0, 3); GL_ASSERT_NO_ERROR;
        LOGFM("Drawing ...");

        glfwSwapBuffers(window);
    }

    rtcReleaseScene(scene);
    rtcReleaseDevice(device);

    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}
