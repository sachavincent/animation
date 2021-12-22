#include "utils.h"
#include "texture.h"
#include "cpu_animator.h"
#include "gpu_animator.h"
#include "dual_gpu_animator.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static void OnWindowResize(GLFWwindow* window, int width, int height)
{
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    WIDTH = width;
    HEIGHT = height;

    glViewport(0, 0, width, height);
}

static void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    app->camera.on_mouse_move(float(xpos), float(ypos));
}

static void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)xoffset;
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    app->camera.on_mouse_scroll(float(yoffset));
}

static void OnKeyFun(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS)
        return;

    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);

    //if(app->mode != Mode::PAUSED)
    app->previous_mode = app->mode;

    switch (key)
    {
    case GLFW_KEY_SPACE:
        if (app->paused)
        {
            app->paused = false;
            app->mode = app->previous_mode;
            app->pause_delay = float(glfwGetTime()) - app->paused_time;
            app->paused_time = 0;
            std::cout << "Animation resumed" << std::endl;
        }
        else
        {
            app->paused = true;
            app->paused_time = float(glfwGetTime());
            app->pause_delay = 0;
            std::cout << "Animation paused" << std::endl;
        }
        break;
    case GLFW_KEY_F1:
        app->mode = Mode::CPU;
        std::cout << "Animation started on CPU" << std::endl;
        break;
    case GLFW_KEY_F2:
        app->mode = Mode::GPU;
        std::cout << "Animation started on GPU" << std::endl;
        break;
    case GLFW_KEY_F3:
        app->mode = Mode::GPU_DUAL;
        std::cout << "Animation started on GPU with dual" << std::endl;
        break;
    }
    if (app->previous_mode != app->mode)
        app->reset = true;
}

static void HandleInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
        return;
    }

    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    FreeCamera& camera = app->camera;
    Mode& mode = app->mode;

    const struct KeyDelta
    {
        int key;
        glm::vec3 delta;
    };
    KeyDelta key_delta[] =
    {
        {GLFW_KEY_W, +camera._front},
        {GLFW_KEY_S, -camera._front},
        {GLFW_KEY_D, +camera._right},
        {GLFW_KEY_A, -camera._right},
        {GLFW_KEY_E, +camera._up},
        {GLFW_KEY_Q, -camera._up},
    };
    for (KeyDelta keyDelta : key_delta)
    {
        if (glfwGetKey(window, keyDelta.key) == GLFW_PRESS)
        {
            camera.on_keyboard_move(keyDelta.delta);
            break;
        }
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    AppState app;
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Projet Animation", nullptr, nullptr);
    assert(window && "Failed to create window.");
    glfwSetWindowUserPointer(window, &app);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, OnWindowResize);
    glfwSetCursorPosCallback(window, OnMouseMove);
    glfwSetScrollCallback(window, OnMouseScroll);
    glfwSetKeyCallback(window, OnKeyFun);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    const int glad_ok = gladLoadGL();
    assert(glad_ok > 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    app.camera.force_refresh();

    Assimp::Importer importer;
    const char* filePath = "model.dae";
    const aiScene* scene = importer.ReadFile(Texture::DIR_PATH + filePath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        exit(1);
    }

    Texture diffuseTexture = Texture("diffuse.png");

    aiMesh* mesh = scene->mMeshes[0];

	AnimPackage CPUAnim = initCPU(scene, mesh);
    CPUAnim.texture = diffuseTexture;

	AnimPackage GPUAnim = initGPU(scene, mesh);
    GPUAnim.texture = diffuseTexture;

	AnimPackage DualGPUAnim = initDualGPU(scene, mesh);
    DualGPUAnim.texture = diffuseTexture;

    float start_time = float(glfwGetTime());
    while (!glfwWindowShouldClose(window))
    {
        const float current_time = float(glfwGetTime());
        if (app.pause_delay != 0)
        {
            start_time += app.pause_delay;
            app.pause_delay = 0;
        }

        if (app.reset && !app.paused)
        {
            start_time = current_time;
            app.reset = false;
        }

        float anim_time = current_time - start_time;

        HandleInput(window);
        if (app.paused)
        {
            anim_time = app.paused_time;
        }
        switch (app.mode)
        {
        case Mode::CPU:
            CPULoop(anim_time, app.camera, CPUAnim);
            break;
        case Mode::GPU:
            GPULoop(anim_time, app.camera, GPUAnim);
            break;
        case Mode::GPU_DUAL:
            DualGPULoop(anim_time, app.camera, DualGPUAnim);
            break;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
}