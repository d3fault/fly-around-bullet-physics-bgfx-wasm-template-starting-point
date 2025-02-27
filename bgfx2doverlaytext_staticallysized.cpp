#include "bgfx2doverlaytext_staticallysized.h"

#include "my_bgfx_view_id_constants.h"
#include "lolreadfile.h"

Bgfx2DOverlayText_StaticallySized::Bgfx2DOverlayText_StaticallySized(float width, float height, float xpos, float ypos, const std::string &text, int fontSize, gil::rgba8_pixel_t fontColor, const std::string &fontPath)
    : image(width, height) //TODO: dynamically choose size based on length of text. wrapping(?). limited ofc by view size
    , vertices{
        { xpos, ypos, 0.0f, 0.0f }, // top-left
        { width+xpos, ypos, 1.0f, 0.0f }, // top-right
        { width+xpos, height+ypos, 1.0f, 1.0f }, // bottom-right
        { xpos, height+ypos, 0.0f, 1.0f }, // bottom-left
      }
    , indices{ 0, 1, 2, 0, 2, 3 }
{
    vs_data = readFile("vs_overlay.bin");
    const bgfx::Memory* vs_mem = bgfx::makeRef(vs_data.data(), vs_data.size());
    vs_handle = bgfx::createShader(vs_mem);

    fs_data = readFile("fs_overlay.bin");
    const bgfx::Memory* fs_mem = bgfx::makeRef(fs_data.data(), fs_data.size());
    fs_handle = bgfx::createShader(fs_mem);

    program_handle = bgfx::createProgram(vs_handle, fs_handle, true);

    vertex_layout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

    const bgfx::Memory* vertex_buffer_mem = bgfx::makeRef(vertices, sizeof(vertices));
    vertex_buffer_handle = bgfx::createVertexBuffer(vertex_buffer_mem, vertex_layout);

    const bgfx::Memory* index_buffer_mem = bgfx::makeRef(indices, sizeof(indices));
    index_buffer_handle = bgfx::createIndexBuffer(index_buffer_mem);

    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

    createImageAndSetupBgfxTexture(text, fontSize, fontColor, fontPath);
}

void Bgfx2DOverlayText_StaticallySized::render(int screenWidth, int screenHeight)
{
    float ortho[16];
    bx::mtxOrtho(ortho,
                 0.0f, float(screenWidth),           // left, right
                 float(screenHeight), 0.0f,          // bottom, top (y flipped)
                 0.0f, 100.0f,                       // near, far
                 0.0f, bgfx::getCaps()->homogeneousDepth);

    bgfx::setViewTransform(MY_BGFX_VIEW_ID_2D_OVERLAY, nullptr, ortho);

    bgfx::setVertexBuffer(0, vertex_buffer_handle);
    bgfx::setIndexBuffer(index_buffer_handle);

    float transform[16];
    bx::mtxTranslate(transform, 0.0f, 0.0f, 0.0f);
    bgfx::setTransform(transform);

    bgfx::setVertexBuffer(0, vertex_buffer_handle);
    bgfx::setIndexBuffer(index_buffer_handle);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::setTexture(0, s_texColor, texture);
    bgfx::submit(MY_BGFX_VIEW_ID_2D_OVERLAY, program_handle);
}

Bgfx2DOverlayText_StaticallySized::~Bgfx2DOverlayText_StaticallySized()
{
    bgfx::destroy(texture);
    bgfx::destroy(s_texColor);
    bgfx::destroy(program_handle);
    bgfx::destroy(vertex_buffer_handle);
    bgfx::destroy(index_buffer_handle);
}

void Bgfx2DOverlayText_StaticallySized::createImageAndSetupBgfxTexture(const std::string &text, int fontSize, gil::rgba8_pixel_t fontColor, const std::string &fontPath)
{
    {
        auto view = gil::view(image);
        fill_pixels(view, gil::rgba8_pixel_t(0, 0, 0, 0)); // Transparent background

        FT_Library ft;
        FT_Init_FreeType(&ft);
        FT_Face face;
        FT_New_Face(ft, fontPath.c_str(), 0, &face);
        FT_Set_Pixel_Sizes(face, 0, fontSize);

        unsigned char userAlpha = gil::at_c<3>(fontColor);

        int x = 10, y = 40;
        for (char c : text) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;
            FT_Bitmap& bitmap = face->glyph->bitmap;

            for (int row = 0; row < bitmap.rows; ++row) {
                for (int col = 0; col < bitmap.width; ++col) {
                    int img_x = x + col;
                    int img_y = y - (face->glyph->bitmap_top - row);
                    if (img_x < image.width() && img_y < image.height()) {
                        unsigned char coverage = bitmap.buffer[row * bitmap.pitch + col];
                        unsigned char alpha = (coverage * userAlpha) / 255;
                        gil::at_c<3>(fontColor) = alpha;
                        view(img_x, img_y) = fontColor;
                    }
                }
            }
            x += face->glyph->advance.x >> 6;
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    } //maybe flush view

    const bgfx::Memory* mem = bgfx::makeRef(interleaved_view_get_raw_data(const_view(image)), image.width() * image.height() * 4);
    texture = bgfx::createTexture2D(image.width(), image.height(), false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
}
