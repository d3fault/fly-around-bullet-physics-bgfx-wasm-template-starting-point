#ifndef SKYBOX_H
#define SKYBOX_H

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>
#include <bimg/bimg.h>

#include <boost/gil.hpp>

class Skybox
{
public:
    Skybox();
    void render(float view[16],  float proj[16]);
    ~Skybox();
private:
    bgfx::TextureHandle loadTexture2D(const std::string &filePath);

    bgfx::VertexLayout layout;
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
    bgfx::ProgramHandle skyboxProgram;
    bgfx::UniformHandle u_viewProjNoTrans;
    bgfx::UniformHandle s_skybox;
    bgfx::TextureHandle skyboxTex;
    boost::gil::rgb8_image_t image;
};

#endif // SKYBOX_H
