#ifndef BGFX2DOVERLAYTEXT_STATICALLY_SIZED_H
#define BGFX2DOVERLAYTEXT_STATICALLY_SIZED_H

#include <vector>
#include <string>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/bx.h>
#undef None
#include <bx/math.h>

#include <boost/gil.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace gil = boost::gil;

class Bgfx2DOverlayText_StaticallySized
{
public:
    struct OverlayVertex {
        float x, y;
        float u, v;
    };

    Bgfx2DOverlayText_StaticallySized(float width, float height, float xpos, float ypos, const std::string &text, int fontSize = 16, gil::rgba8_pixel_t fontColor = gil::rgba8_pixel_t(0, 0, 0, 255)/*black*/, const std::string &fontPath = "Vegur-Regular.ttf");
    void render(int screenWidth, int screenHeight);
    ~Bgfx2DOverlayText_StaticallySized();

    std::vector<char> vs_data;
    bgfx::ShaderHandle vs_handle;
    std::vector<char> fs_data;
    bgfx::ShaderHandle fs_handle;
    bgfx::ProgramHandle program_handle;
    bgfx::VertexLayout vertex_layout;
    bgfx::VertexBufferHandle vertex_buffer_handle;
    bgfx::IndexBufferHandle index_buffer_handle;
    bgfx::UniformHandle s_texColor;
    const OverlayVertex vertices[4];
    const uint16_t indices[6];

    gil::rgba8_image_t image;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
private:
    void createImageAndSetupBgfxTexture(const std::string &text, int fontSize, gil::rgba8_pixel_t fontColor, const std::string &fontPath);
};

#endif // BGFX2DOVERLAYTEXT_STATICALLY_SIZED_H
