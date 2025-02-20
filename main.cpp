#include <iostream>
#include <vector>
#include <fstream>
#include <random>
#include <unordered_set>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include "bx/math.h"

#if __EMSCRIPTEN__

#include <emscripten.h>
#include <emscripten/html5.h>
#include "GLFW/glfw3.h"
#define MY_EMSCRIPTEN_CANVAS_CSS_SELECTOR "#canvas"

#else // Linux/X11

#include <chrono>
#include <algorithm>
#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_X11
#include "GLFW/glfw3native.h"

#endif // __EMSCRIPTEN__

#include <btBulletDynamicsCommon.h>
#include <btBulletCollisionCommon.h>

//TODO: auto-detect screen dimensions
const int screenWidth = 1280;
const int screenHeight = 720;

std::vector<char> vertexShaderCode;
bgfx::ShaderHandle vertexShader;
std::vector<char> fragmentShaderCode;
bgfx::ShaderHandle fragmentShader;
unsigned int counter = 0;
bgfx::VertexBufferHandle vbh;
bgfx::IndexBufferHandle ibh;
bgfx::ProgramHandle program;

bool pointerLocked = false;
double lastMouseX = 0.0f;
double lastMouseY = 0.0f;
float cameraPitch = 0.0f;
float cameraYaw = 0.0f;
const float mouseSensitivity = 0.1f;
bx::Vec3 eye = {0.0f, 0.0f, -5.0f};

std::unordered_set<int> pressedKeys;
const float moveSpeed = 0.1f;

const double TARGET_FPS = 60.0;
const double FRAME_DURATION = 1.0 / TARGET_FPS;

struct PosColorVertex
{
    float x;
    float y;
    float z;
    uint32_t abgr;
};

static PosColorVertex cubeVertices[] =
{
    {-1.0f,  1.0f,  1.0f, 0xff000000 },
    { 1.0f,  1.0f,  1.0f, 0xff0000ff },
    {-1.0f, -1.0f,  1.0f, 0xff00ff00 },
    { 1.0f, -1.0f,  1.0f, 0xff00ffff },
    {-1.0f,  1.0f, -1.0f, 0xffff0000 },
    { 1.0f,  1.0f, -1.0f, 0xffff00ff },
    {-1.0f, -1.0f, -1.0f, 0xffffff00 },
    { 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static const uint16_t cubeTriList[] =
{
    0, 1, 2,
    1, 3, 2,
    4, 6, 5,
    5, 6, 7,
    0, 2, 4,
    4, 2, 6,
    1, 5, 3,
    5, 7, 3,
    0, 4, 1,
    4, 5, 1,
    2, 3, 6,
    6, 3, 7,
};

struct PhysicsWorld {
    btDefaultCollisionConfiguration* collisionConfig;
    btCollisionDispatcher* dispatcher;
    btBroadphaseInterface* broadphase;
    btSequentialImpulseConstraintSolver* solver;
    btDiscreteDynamicsWorld* dynamicsWorld;

    btCollisionShape* cubeShape;
    btRigidBody* cubeRigidBody = nullptr;
    btCollisionShape* floorShape;
    btRigidBody* floorRigidBody;

    PhysicsWorld() {
        collisionConfig = new btDefaultCollisionConfiguration();
        dispatcher = new btCollisionDispatcher(collisionConfig);
        broadphase = new btDbvtBroadphase();
        solver = new btSequentialImpulseConstraintSolver();
        dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);
        dynamicsWorld->setGravity(btVector3(0, -9.8, 0));

        // Create cube shape and rigid body
        cubeShape = new btBoxShape(btVector3(1.0f, 1.0f, 1.0f)); // Adjust size as needed
        btScalar mass = 1.0f;
        btVector3 inertia(0, 0, 0); // Declare inertia
        cubeShape->calculateLocalInertia(mass, inertia);
        createCubeRigidBodyWithRandomRotation(mass, inertia);

        // Create floor shape and rigid body (static)
        floorShape = new btBoxShape(btVector3(10.0f, 0.5f, 10.0f)); // Large floor
        btDefaultMotionState* floorMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -2.0f, 0))); // Low position
        btRigidBody::btRigidBodyConstructionInfo floorRigidBodyCI(0, floorMotionState, floorShape, btVector3(0, 0, 0)); // Mass 0 for static
        floorRigidBody = new btRigidBody(floorRigidBodyCI);
        dynamicsWorld->addRigidBody(floorRigidBody);
    }

    void resetCube()
    {
        dynamicsWorld->removeRigidBody(cubeRigidBody);

        btScalar mass = 1.0f;
        btVector3 inertia(0, 0, 0);
        cubeShape->calculateLocalInertia(mass, inertia);
        createCubeRigidBodyWithRandomRotation(mass, inertia);

        cubeRigidBody->activate(true);
    }

    ~PhysicsWorld() {
        delete dynamicsWorld;
        delete solver;
        delete broadphase;
        delete dispatcher;
        delete collisionConfig;

        delete cubeShape;
        delete floorShape;
        delete cubeRigidBody->getMotionState();
        delete cubeRigidBody;
        delete floorRigidBody->getMotionState();
        delete floorRigidBody;
    }
private:
    void createCubeRigidBodyWithRandomRotation(btScalar mass, btVector3 inertia)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> distrib(0.0, 2.0 * M_PI);

        btQuaternion randomRotation(distrib(gen), distrib(gen), distrib(gen), 1.0f);
        randomRotation.normalize();

        btDefaultMotionState* cubeMotionState = new btDefaultMotionState(btTransform(randomRotation, btVector3(0, 5, 0)));

        btRigidBody::btRigidBodyConstructionInfo cubeRigidBodyCI(mass, cubeMotionState, cubeShape, inertia);
        btRigidBody* newCubeRigidBody = new btRigidBody(cubeRigidBodyCI);

        dynamicsWorld->addRigidBody(newCubeRigidBody);

        if(cubeRigidBody)
        {
            delete cubeRigidBody->getMotionState();
            delete cubeRigidBody;
        }
        cubeRigidBody = newCubeRigidBody;
    }
};

PhysicsWorld physicsWorld;

std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        std::string error = "failed to open file: " + filename;
        throw std::runtime_error(error);
    }

    std::size_t fileSize = (std::size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

#ifdef __EMSCRIPTEN__
EM_BOOL pointerlock_callback(int eventType, const EmscriptenPointerlockChangeEvent* event, void* userData) {
    if (event->isActive) {
        std::cout << "Pointer lock activated." << std::endl;
        pointerLocked = true;
    } else {
        std::cout << "Pointer lock released." << std::endl;
        pointerLocked = false;
    }
    return EM_TRUE; // Indicate the event was handled
}
#endif // __EMSCRIPTEN__

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        switch(key)
        {
#ifndef __EMSCRIPTEN__
        // Linux/X11
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
#endif
        case GLFW_KEY_E:
            physicsWorld.resetCube();
            break;
        case GLFW_KEY_ESCAPE:
#ifdef __EMSCRIPTEN__
            //unlock pointer lock
            emscripten_exit_pointerlock();
            std::cout << "manually requesting emscripten_exit_pointerlock. i'm not sure if this will be called because ESC *always* releases pointer lock" << std::endl;
#else //Linux/X11
            //TODO: Linux/X11
            //to decrease dupe code, maybe just call the emscripten pointerlock_callback here (but also disable cursor hiding and mouse recentering)
            pointerLocked = false;
#endif // __EMSCRIPTEN__
            break;
        default:
            pressedKeys.insert(key);
            break;
        }
    }
    else if(action == GLFW_RELEASE)
    {
        pressedKeys.erase(key);
    }
}
static void mouse_click_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
#ifdef __EMSCRIPTEN__
        EMSCRIPTEN_RESULT result = emscripten_request_pointerlock(MY_EMSCRIPTEN_CANVAS_CSS_SELECTOR, EM_FALSE);
        if (result == EMSCRIPTEN_RESULT_SUCCESS) {
            std::cout << "Pointer lock requested successfully." << std::endl;
        } else {
            std::cout << "Failed to request pointer lock, error code: " << result << std::endl;
        }
#else
        //TODO: Linux/X11
        //to decrease dupe code, maybe just call the emscripten pointerlock_callback here (but also set up cursor hiding and mouse recentering)
        pointerLocked = true;
#endif // __EMSCRIPTEN__
    }
}

void mouse_move_callback(GLFWwindow* window, double xpos, double ypos)
{
    if(pointerLocked)
    {
        double dx = xpos - lastMouseX;
        double dy = ypos - lastMouseY;

        cameraYaw += dx * mouseSensitivity;
        cameraPitch -= dy * mouseSensitivity;

        cameraPitch = bx::clamp(cameraPitch, -89.0f, 89.0f);
    }
    lastMouseX = xpos;
    lastMouseY = ypos;
}

void renderFrame() {
    float radYaw = bx::toRad(cameraYaw);
    float radPitch = bx::toRad(cameraPitch);

    bx::Vec3 forward = {0.0f, 0.0f, 1.0f};
    forward.x = cosf(radPitch) * sinf(radYaw);
    forward.y = sinf(radPitch);
    forward.z = cosf(radPitch) * cosf(radYaw);
    forward = bx::normalize(forward);
    bx::Vec3 right = bx::normalize(bx::cross({0.0f, 1.0f, 0.0f}, forward));

    // WASD movement
    bx::Vec3 moveDirection = {0.0f, 0.0f, 0.0f};
    if (pressedKeys.count(GLFW_KEY_W) || pressedKeys.count(GLFW_KEY_UP)) {
        moveDirection = bx::add(moveDirection, forward);
    }
    if (pressedKeys.count(GLFW_KEY_S) || pressedKeys.count(GLFW_KEY_DOWN)) {
        moveDirection = bx::sub(moveDirection, forward);
    }
    if (pressedKeys.count(GLFW_KEY_A) || pressedKeys.count(GLFW_KEY_LEFT)) {
        moveDirection = bx::sub(moveDirection, right);
    }
    if (pressedKeys.count(GLFW_KEY_D) || pressedKeys.count(GLFW_KEY_RIGHT)) {
        moveDirection = bx::add(moveDirection, right);
    }

    if (bx::length(moveDirection) > 0.0f) {
        moveDirection = bx::normalize(moveDirection);
        eye = bx::add(eye, bx::mul(moveDirection, moveSpeed));
    }

    bx::Vec3 at = bx::add(eye, forward);
    float view[16];
    bx::mtxLookAt(view, eye, at);

    float proj[16];
    bx::mtxProj(proj, 60.0f, float(screenWidth) / float(screenHeight), 0.05f, 150.0f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(0, view, proj);

    // Update physics simulation
    physicsWorld.dynamicsWorld->stepSimulation(1.0f / 60.0f, 10);

    // Cube 1 (Physics-driven with rotation)
    btTransform trans;
    physicsWorld.cubeRigidBody->getMotionState()->getWorldTransform(trans);
    btVector3 pos = trans.getOrigin();
    btQuaternion rot = trans.getRotation();
    btTransform combinedTransform;
    combinedTransform.setIdentity();
    combinedTransform.setOrigin(pos);
    combinedTransform.setRotation(rot);
    btMatrix3x3 rotMatrix = combinedTransform.getBasis();
    btVector3 transVec = combinedTransform.getOrigin();
    float finalMatrix1[16];
    rotMatrix.getOpenGLSubMatrix(finalMatrix1);
    finalMatrix1[12] = transVec.x();
    finalMatrix1[13] = transVec.y();
    finalMatrix1[14] = transVec.z();
    finalMatrix1[15] = 1.0f;

    bgfx::setTransform(finalMatrix1);
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);
    bgfx::submit(0, program);

    // Cube 2 (Floor - Static)
    btScalar scaleX2 = 10.0f;
    btScalar scaleY2 = 0.5f;
    btScalar scaleZ2 = 10.0f;
    btMatrix3x3 scaleMatrix2(scaleX2, 0, 0, 0, scaleY2, 0, 0, 0, scaleZ2);
    btVector3 translateVector2(0.0f, -2.0f, 0.0f);
    btTransform combinedTransform2;
    combinedTransform2.setIdentity();
    combinedTransform2.setBasis(scaleMatrix2);
    combinedTransform2.setOrigin(translateVector2);
    btMatrix3x3 rotMatrix2 = combinedTransform2.getBasis();
    btVector3 transVec2 = combinedTransform2.getOrigin();
    float finalMatrix2[16];
    rotMatrix2.getOpenGLSubMatrix(finalMatrix2);
    finalMatrix2[12] = transVec2.x();
    finalMatrix2[13] = transVec2.y();
    finalMatrix2[14] = transVec2.z();
    finalMatrix2[15] = 1.0f;

    bgfx::setTransform(finalMatrix2);
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);
    bgfx::submit(0, program);

    bgfx::frame();
    counter++;
}

int main(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Let bgfx handle rendering API
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Hello, bullet physics + bgfx + wasm! Click anywhere to restart", NULL, NULL);
    if(!window)
    {
        glfwTerminate();
        return 1;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, EM_TRUE, pointerlock_callback);
#endif // __EMSCRIPTEN__

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_click_callback);
    glfwSetCursorPosCallback(window, mouse_move_callback);

    bgfx::PlatformData platformData{};

#ifdef __EMSCRIPTEN__
    platformData.nwh = (void*)MY_EMSCRIPTEN_CANVAS_CSS_SELECTOR;
#else // Linux/X11
    platformData.nwh = (void*)uintptr_t(glfwGetX11Window(window));
#endif // __EMSCRIPTEN__

    platformData.context = NULL;
    platformData.backBuffer = NULL;
    platformData.backBufferDS = NULL;

    bgfx::Init init;
    init.type = bgfx::RendererType::OpenGL;

    init.resolution.width = screenWidth;
    init.resolution.height = screenHeight;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.platformData = platformData;

    if (!bgfx::init(init))
    {
        throw std::runtime_error("Failed to initialize bgfx");
    }

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, screenWidth, screenHeight);

    bgfx::VertexLayout pcvDecl;
    pcvDecl.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
    vbh = bgfx::createVertexBuffer(bgfx::makeRef(cubeVertices, sizeof(cubeVertices)), pcvDecl);
    ibh = bgfx::createIndexBuffer(bgfx::makeRef(cubeTriList, sizeof(cubeTriList)));

    vertexShaderCode = readFile("vs_cubes.bin");
    vertexShader = bgfx::createShader(bgfx::makeRef(vertexShaderCode.data(), vertexShaderCode.size()));
    if (!bgfx::isValid(vertexShader))
    {
        throw std::runtime_error("Failed to create vertex shader");
    }
    else
    {
        std::cout << "Vertex shader load success!" << std::endl;
    }

    fragmentShaderCode = readFile("fs_cubes.bin");
    fragmentShader = bgfx::createShader(bgfx::makeRef(fragmentShaderCode.data(), fragmentShaderCode.size()));
    if (!bgfx::isValid(fragmentShader))
    {
        throw std::runtime_error("Failed to create fragment shader");
    }
    else
    {
        std::cout << "Fragment shader load success!" << std::endl;
    }
    program = bgfx::createProgram(vertexShader, fragmentShader, true);

    if (!bgfx::isValid(program))
    {
        throw std::runtime_error("Failed to create program");
    }
    else {
        std::cout << "Shader program create success!" << std::endl;
    }

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);

#if __EMSCRIPTEN__
    emscripten_set_main_loop(renderFrame, 0, true);
#else // Linux/X11
    while(!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        renderFrame();
        // Wait until FRAME_DURATION has passed while handling events
        double elapsedSeconds = 0.0;
        do {
            auto currentTime = std::chrono::high_resolution_clock::now();
            elapsedSeconds = std::chrono::duration<double>(currentTime - frameStart).count();

            glfwWaitEventsTimeout(std::max(0.01, FRAME_DURATION - elapsedSeconds));

            currentTime = std::chrono::high_resolution_clock::now();
            elapsedSeconds = std::chrono::duration<double>(currentTime - frameStart).count();
        } while (elapsedSeconds < FRAME_DURATION);
    }

    bgfx::destroy(program);
    bgfx::destroy(fragmentShader);
    bgfx::destroy(vertexShader);
    bgfx::destroy(ibh);
    bgfx::destroy(vbh);
    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
#endif // __EMSCRIPTEN__
}
