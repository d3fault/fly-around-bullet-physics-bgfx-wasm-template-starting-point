#include "skybox.h"

#include <boost/gil/extension/io/jpeg.hpp>
#include <iostream>
#include <stdexcept>

#include "my_bgfx_view_id_constants.h"
#include "lolreadfile.h"

// Vertex structure/layout
struct SkyboxPosVertex {
    float x, y, z;
};

static SkyboxPosVertex s_SkyboxCubeVertices[] = {
    {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f},
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
    {-1.0f,  1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}
};

static const uint16_t s_SkyboxCubeIndices[] = {
    0, 2, 1, 1, 2, 3,
    4, 5, 6, 5, 7, 6,
    0, 4, 2, 4, 6, 2,
    1, 3, 5, 5, 3, 7,
    0, 1, 4, 4, 1, 5,
    2, 6, 3, 6, 7, 3
};

// Helper to load an image using Boost.GIL
static boost::gil::rgb8_image_t loadImage(const std::string &filename) {
    boost::gil::rgb8_image_t img;
    boost::gil::read_image(filename, img, boost::gil::jpeg_tag());
    return img;
}

// Helper to load a 2D texture
bgfx::TextureHandle Skybox::loadTexture2D(const std::string &filePath) {
    image = loadImage(filePath);
    auto view = boost::gil::const_view(image);
    int width = view.width();
    int height = view.height();

    const bgfx::Memory* mem = bgfx::copy(boost::gil::interleaved_view_get_raw_data(view), width * height * 3);
    return bgfx::createTexture2D(uint16_t(width), uint16_t(height), false, 1, bgfx::TextureFormat::RGB8, BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, mem);
}

// Helper to load a binary shader file into bgfx memory
static bgfx::ShaderHandle loadShader(const std::string &filename) {
    std::vector<char> shaderSource = readFile(filename);
    shaderSource.push_back('\0');

    const bgfx::Memory* mem = bgfx::copy(shaderSource.data(), shaderSource.size());
    return bgfx::createShader(mem);
}

// Helper to create a bgfx program (vertex + fragment)
static bgfx::ProgramHandle loadProgram(const std::string &vsName, const std::string &fsName) {
    bgfx::ShaderHandle vsh = loadShader(vsName);
    bgfx::ShaderHandle fsh = loadShader(fsName);

    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        std::cerr << "Failed to load shaders." << std::endl;
        return BGFX_INVALID_HANDLE;
    }

    return bgfx::createProgram(vsh, fsh, true);
}

Skybox::Skybox() {
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();

    vbh = bgfx::createVertexBuffer(bgfx::makeRef(s_SkyboxCubeVertices, sizeof(s_SkyboxCubeVertices)), layout);
    ibh = bgfx::createIndexBuffer(bgfx::makeRef(s_SkyboxCubeIndices, sizeof(s_SkyboxCubeIndices)));

    skyboxProgram = loadProgram("vs_skybox.bin", "fs_skybox.bin");
    if (!bgfx::isValid(skyboxProgram)) {
        throw std::runtime_error("Skybox program creation failed.");
    }

    u_viewProjNoTrans = bgfx::createUniform("u_viewProjNoTrans", bgfx::UniformType::Mat4);
    s_skybox = bgfx::createUniform("s_skybox", bgfx::UniformType::Sampler);
    skyboxTex = loadTexture2D("skybox.jpg");

    if (!bgfx::isValid(skyboxTex)) {
        throw std::runtime_error("Failed to load skybox.jpg texture.");
    }
}

void Skybox::render(float view[16], float proj[16]) {
    float viewNoTrans[16];
    bx::memCopy(viewNoTrans, view, sizeof(viewNoTrans));
    viewNoTrans[12] = 0.0f;
    viewNoTrans[13] = 0.0f;
    viewNoTrans[14] = 0.0f;

    float viewProjNoTrans[16];
    bx::mtxMul(viewProjNoTrans, viewNoTrans, proj);

    bgfx::setViewTransform(MY_BGFX_VIEW_ID_SKYBOX, viewNoTrans, proj);
    bgfx::setUniform(u_viewProjNoTrans, viewProjNoTrans);
    bgfx::setVertexBuffer(0, vbh);
    bgfx::setIndexBuffer(ibh);

    float scale = 50.0f;
    float mtx[16];
    bx::mtxScale(mtx, scale);
    bgfx::setTransform(mtx);

    bgfx::setTexture(0, s_skybox, skyboxTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB);
    bgfx::submit(MY_BGFX_VIEW_ID_SKYBOX, skyboxProgram);
}

Skybox::~Skybox() {
    bgfx::destroy(skyboxTex);
    bgfx::destroy(skyboxProgram);
    bgfx::destroy(vbh);
    bgfx::destroy(ibh);
    bgfx::destroy(u_viewProjNoTrans);
    bgfx::destroy(s_skybox);
}
