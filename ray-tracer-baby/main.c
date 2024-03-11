#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>

#include <pthread.h>
#include <unistd.h>

#include <embree3/rtcore.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include <stb/stb_image_write.h>
#include <cmm/cmm.h>

#include "renderer.h"
#include "shaders.h"
#include "ray_tracing.h"
#include "progress_bar.h"


#define RNG_SEED 42

#define N_INSTANCES 50
#define X 0
#define Y 1
#define Z 2

internal struct Config {
    int glVersionMajor;
    int glVersionMinor;
    usize nWorkers;
    const char* const title;
} Config = {
    .glVersionMajor = 4,
    .glVersionMinor = 2,
    .nWorkers = 8,
    .title = "ray-tracer-baby",
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
    bool windowedMode;
    bool cursorDisabled;
    GLFWwindow* window;
    f64 xPos;
    f64 yPos;
} AppState = {
    .width = 800,
    .height = 800,
    .glInitialized = false,
    .cursorDisabled = false,
    .windowedMode = false
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
    const u32 err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("OpenGL error %08x, at %s:%i - for %s\n", err, fname, line, stmt);
        abort();
    }
}

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

u32 createGroundPlane (RTCDevice device, RTCScene scene) {
    /* create a triangulated plane with 2 triangles and 4 vertices */
    RTCGeometry geom = rtcNewGeometry (device, RTC_GEOMETRY_TYPE_TRIANGLE);

    /* set vertices */
    Position* vertices = (Position*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_VERTEX,0,RTC_FORMAT_FLOAT3,sizeof(Position),4);
    vertices[0].x = -10; vertices[0].y = -2; vertices[0].z = -10;
    vertices[1].x = -10; vertices[1].y = -2; vertices[1].z = +10;
    vertices[2].x = +10; vertices[2].y = -2; vertices[2].z = -10;
    vertices[3].x = +10; vertices[3].y = -2; vertices[3].z = +10;

    /* set triangles */
    vec3* triangles = (vec3*) rtcSetNewGeometryBuffer(geom,RTC_BUFFER_TYPE_INDEX,0,RTC_FORMAT_UINT3,sizeof(vec3),2);
    triangles[0][0] = 0; triangles[0][1] = 1; triangles[0][2] = 2;
    triangles[1][0] = 1; triangles[1][1] = 3; triangles[1][2] = 2;

    rtcCommitGeometry(geom);
    u32 geomID = rtcAttachGeometry(scene,geom);
    rtcReleaseGeometry(geom);
    return geomID;
}

typedef struct {
    RayTracer* rt;
    Buffer2d framebuffer;
    usize tid;
    PTask* task;
} RenderJobParams;

DeclareArray(RenderJobParams);

void RenderRange(const RayTracer* const rt, Buffer2d framebuffer, usize initialRow, usize nRows, PTask* task) {
    for (usize y = initialRow; y < initialRow + nRows; y++) {
        task->progress += 1;
        for (usize x = 0; x < framebuffer.width; x++) {
            struct RTCRayHit rayhit;
            f32 accumulator[3] = { 0.f };
            vec3 color;

            for (usize ri = 0; ri < rt->nRaysPerSample; ri++) {
                rayhit.ray.org_x = 0.0f;
                rayhit.ray.org_y = 0.0f;
                rayhit.ray.org_z = 1.0f;

                rayhit.ray.dir_x = (2.0f * ((f32)x / AppState.width)) - 1.0f;
                rayhit.ray.dir_y = 1.0f - (2.0f * ((f32)y / AppState.height));
                rayhit.ray.dir_z = -1.0f;
                rayhit.ray.tnear = 0.001f;
                rayhit.ray.tfar = INFINITY;
                rayhit.ray.mask = 0xFFFFFFFF;
                rayhit.ray.flags = 0;
                rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
                
                glm_vec3_one(color);
                TraceRay(rt, &rayhit, 0, color);
                
                accumulator[X] += color[X];
                accumulator[Y] += color[Y];
                accumulator[Z] += color[Z];
            }
            glm_vec3_scale(accumulator, 1.f / rt->nRaysPerSample, accumulator);
            framebuffer.buffer[y * framebuffer.width + x][X] = (u8)(accumulator[X] * 255.999f);
            framebuffer.buffer[y * framebuffer.width + x][Y] = (u8)(accumulator[Y] * 255.999f);
            framebuffer.buffer[y * framebuffer.width + x][Z] = (u8)(accumulator[Z] * 255.999f);
        }
    }
}

internal void* RenderJob(void* args) {
    RenderJobParams* const params = args;

    ASSERT_EQ(params->framebuffer.height % Config.nWorkers, 0);
    const usize offset = params->framebuffer.height / Config.nWorkers;

    const usize initialRow = params->tid * offset;
    params->task->end = offset;
    params->task->progress = 0;
    RenderRange(params->rt, params->framebuffer, initialRow, offset, params->task);
    return NULL;
}

DeclareArray(pthread_t);

void SceneNxN(Instances instances, usize n) {
    const isize h = n / 2;
    Material _;
    Transform transform;
    for (usize i = 0; i < n * n; i++) {
        const f32 norm = (f32)i / (f32)(n * n);
        const isize xi = i % n - h;
        const isize yi = (i / n) % n - h;
        const isize zi = -1;

        glm_vec3_copy((vec3) { (f32)xi, (f32)yi, (f32)zi + 0.3 }, transform.translation);
        glm_vec3_copy((vec3) { 0.f, norm * M_PI, 0.f }, transform.rotation);
        glm_vec3_copy((vec3) { 0.3f, 0.3f, 0.3f }, transform.scale);
        CreateInstance(
            &transform,
            &_,
            &instances.data[i]
        );
    }
}

i32 main(void) {
    PRINTLN(FS(usize), __STDC_VERSION__);
    const char* const objPaths[] = { "../scenes/backpack.obj" };

    if (AppState.windowedMode) {
        LOGLNM("Running in windowed mode");

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

        GLFWwindow* const window = glfwCreateWindow((i32)AppState.width, (i32)AppState.height, Config.title, NULL, NULL);
        
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

        glfwSetKeyCallback(window, HandleKey);
        glfwSetCursorPosCallback(window, HandleMouse);
        ToggleCursorMode();

        glfwGetCursorPos(window, &AppState.xPos, &AppState.yPos);
        glfwSwapInterval(0);
        PRINTLN("Initialized OpenGL context: %s core", (const char*) glGetString(GL_VERSION));
    } else {
        LOGLNM("Running in console mode");
    }

    const RendererConfig config = {
        .nMeshes = ARRAY_LENGTH(objPaths),
        .objPaths = objPaths,
        .vs = vs,
        .fs = fs,
        .useGl = AppState.windowedMode,
    };
    Renderer.initialize(config);

    usize frameCount = 0;
    f64 lastUpdate = 0.0;

    COMMENT(--------========[ Create Instances ]========--------)

    const usize nInstancesInRow = 3;
    const usize nInstances = nInstancesInRow * nInstancesInRow;

    Transform transform = {};
    MaterialRaster material;
    Instances instances = AllocateArray(Instance, nInstances);
    SceneNxN(instances, nInstancesInRow);

    if (AppState.windowedMode) AddInstances(0, instances);

    const RTCDevice device = rtcNewDevice(NULL);
    if (!device) PANIC("error %d: cannot create device", rtcGetDeviceError(NULL));
    rtcSetDeviceErrorFunction(device, EmbreeErrorCallback, NULL);

    if (rtcGetDeviceProperty(device, RTC_DEVICE_PROPERTY_RAY_MASK_SUPPORTED)) 
        LOGLNM("RTC_DEVICE_PROPERTY_RAY_MASK_SUPPORTED is on");

    RTCScene scene = rtcNewScene(device);

    RTCScene meshScene = rtcNewScene(device);
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

    for (usize i = 0; i < Renderer.meshes.data[0].obj.nIndices / 3; i++) {
        indices[i][0] = Renderer.meshes.data[0].obj.indices[3 * i + 0];
        indices[i][1] = Renderer.meshes.data[0].obj.indices[3 * i + 1];
        indices[i][2] = Renderer.meshes.data[0].obj.indices[3 * i + 2];
    }

    rtcCommitGeometry(mesh);

    // Attach the geometry to the scene
    rtcAttachGeometry(meshScene, mesh);
    rtcReleaseGeometry(mesh);
    rtcCommitScene(meshScene);

    // Create Embree instnaces
    for (usize i = 0; i < instances.len; i++) {
        RTCGeometry geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(geometry, meshScene);
        // glm_mat4_transpose(instances.data[i].model);
        rtcSetGeometryTransform(
            geometry,
            0,
            RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
            instances.data[i].model
        );
        rtcCommitGeometry(geometry);
        rtcAttachGeometry(scene, geometry);
        rtcReleaseGeometry(geometry);
    }
    
    // Commit the scene
    rtcCommitScene(scene);

    COMMENT(---------===========[ Trace Rays ]===========---------)

    RayTracer rt = (RayTracer) {
        .materials = AllocateArray(Material, instances.len),
        // .nMaxReflections = 1,
        // .nRaysPerSample = 1,
        .nMaxReflections = 15,
        .nRaysPerSample = 30,
        .rtcScene = scene,
        .skyColor = { 0.5f, 0.7f, 1.0f },
    };

    for (usize i = 0; i < instances.len; i++) {
        CreateLambertian(&rt.materials.data[i], Palette1[i % ARRAY_LENGTH(Palette1)], 0.8f);
    }

    Array(Rgb256) buffer = AllocateArray(Rgb256, AppState.width * AppState.height);
    Buffer2d framebuffer = (Buffer2d) {
        .width = AppState.width,
        .height = AppState.height,
        .buffer = buffer.data
    };

    Tasks tasks = {
        .pTasks = AllocateArray(PTask, Config.nWorkers),
        .sTasks = (Array(STask)) { .len = 0 }
    };

    Display display = {
        .tasks = tasks,
        .start = time(NULL)
    };

    vec3 lookDir;
    glm_vec3_copy(Renderer.camera.direction, lookDir);

    Array(RenderJobParams) params = AllocateArray(RenderJobParams, Config.nWorkers);
    Array(pthread_t) tids = AllocateArray(pthread_t, Config.nWorkers);

    LOGLN("Starting" FS(usize) "worker threads", Config.nWorkers);
    for (usize tid = 0; tid < Config.nWorkers; tid++) {
        params.data[tid].tid = tid;
        params.data[tid].framebuffer = framebuffer;
        params.data[tid].rt = &rt;
        params.data[tid].task = &display.tasks.pTasks.data[tid];
        LOGLN("  * starting worker:" FS(usize), tid);
        const i32 result = pthread_create(
            &tids.data[tid],
            NULL,
            RenderJob,
            &params.data[tid]
        );
        if (0 != result) PANIC("Failed to create worker" FS(usize), tid);
    }
    LOGLNM("Waiting for worker threads to finish");
    SetupDisplay(&display);
    while(!FinishedDisplay(&display)) {
        usleep(16 * 1000);
        UpdateDisplay(&display);
    }

    // TODO: implement concurrent prograss bars
    for (usize tid = 0; tid < Config.nWorkers; tid++) {
        pthread_join(tids.data[tid], NULL);
    }
    
    LOGLNM("Tracing done");

    i32 result = stbi_write_bmp("./test.bmp", framebuffer.width, framebuffer.height, 3, framebuffer.buffer);
    if (result == 0) PANICM("image failed");
    else LOGLNM("Image written to file");

    if (AppState.windowedMode) {
        u32 texture;
        glGenTextures(1, &texture); GL_ASSERT_NO_ERROR;
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, AppState.width, AppState.height, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer.buffer); GL_ASSERT_NO_ERROR;

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

        while(true) { 
            glDrawArrays(GL_TRIANGLES, 0, 3); GL_ASSERT_NO_ERROR;
            LOGFM("Drawing ...");

            glfwSwapBuffers(AppState.window);
        }

        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(program);

        glfwDestroyWindow(AppState.window);
        glfwTerminate();
    }

    FreeArray(tasks.pTasks);
    FreeArray(buffer);

    rtcReleaseScene(scene);
    rtcReleaseScene(meshScene);
    rtcReleaseDevice(device);
    exit(EXIT_SUCCESS);
}
