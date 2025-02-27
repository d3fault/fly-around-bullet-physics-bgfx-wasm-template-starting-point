#include "bgfx2doverlaytext_dynamicallysized.h"

#include <cmath>
#include <algorithm>

#include "my_bgfx_view_id_constants.h"
#include "lolreadfile.h"

Bgfx2DOverlayText_DynamicallySized::Bgfx2DOverlayText_DynamicallySized(float xpos, float ypos, const std::string &text, int fontSize, gil::rgba8_pixel_t fontColor, int minWidth, const std::string &fontPath) //TODO: dynamically choose size based on length of text. wrapping(?). limited ofc by view size
    : indices{ 0, 1, 2, 0, 2, 3 }
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

    createImageAndSetupBgfxTexture(text, fontPath, fontSize, fontColor, minWidth);

    OverlayVertex temp[] = {
        { xpos, ypos, 0.0f, 0.0f }, // top-left
        { image.width()+xpos, ypos, 1.0f, 0.0f }, // top-right
        { image.width()+xpos, image.height()+ypos, 1.0f, 1.0f }, // bottom-right
        { xpos, image.height()+ypos, 0.0f, 1.0f }, // bottom-left
    };
    std::memcpy(vertices, temp, sizeof(temp)); //TODO: don't do this
}

void Bgfx2DOverlayText_DynamicallySized::render(int screenWidth, int screenHeight)
{
    float ortho[16];
    bx::mtxOrtho(ortho,
                 0.0f, float(screenWidth),           // left, right
                 float(screenHeight), 0.0f,          // bottom, top (y flipped)
                 0.0f, 100.0f,                       // near, far
                 0.0f, bgfx::getCaps()->homogeneousDepth);
    //bgfx::setViewRect(1, 0, 0, screenWidth, screenHeight);
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


Bgfx2DOverlayText_DynamicallySized::~Bgfx2DOverlayText_DynamicallySized()
{
    bgfx::destroy(texture);
    bgfx::destroy(s_texColor);
    bgfx::destroy(program_handle);
    bgfx::destroy(vertex_buffer_handle);
    bgfx::destroy(index_buffer_handle);
}
// Helper struct to hold line metrics
struct LineMetrics {
    int width;
    int height;
    std::string text;
};

/// Render a single line of text onto a raw RGBA8 buffer using FreeType
///  x, y specify the baseline from which to render (top-left corner).
///  'bufferWidth' and 'bufferHeight' are the total dimensions of the buffer.
///  'pitch' is how many bytes one scanline takes in memory (usually 4 * bufferWidth).
///
/// This is the low-level routine that:
///   1) Loads each glyph,
///   2) Renders into the FreeType bitmap,
///   3) Blends the rendered glyph into the RGBA buffer.
void draw_text_line(FT_Face face,
                    gil::rgba8_view_t& view,
                    const std::string& textLine,
                    int startX, int baselineY,
                    gil::rgba8_pixel_t color)
{
    int xOffset = startX;

    for (char c : textLine) {
        // Convert character to glyph index
        FT_UInt glyphIndex = FT_Get_Char_Index(face, c);
        if (glyphIndex == 0) {
            // character not found, skip
            continue;
        }

        // Load the glyph (render as 8-bit gray, for instance)
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
            continue;
        }

        FT_Bitmap &bitmap = face->glyph->bitmap;

        int glyphLeft   = xOffset + face->glyph->bitmap_left;
        int glyphTop    = baselineY - face->glyph->bitmap_top;
        int glyphWidth  = bitmap.width;
        int glyphHeight = bitmap.rows;

        unsigned char userAlpha = gil::at_c<3>(color);

        // Blend the glyph's bitmap into our RGBA image
        for (int row = 0; row < glyphHeight; ++row) {
            for (int col = 0; col < glyphWidth; ++col) {
                // Grayscale coverage from FreeType
                unsigned char coverage = bitmap.buffer[row * bitmap.pitch + col];

                if (coverage > 0) {
                    int x = glyphLeft + col;
                    int y = glyphTop + row;
                    if (x >= 0 && x < view.width() && y >= 0 && y < view.height()) {
                        // Current pixel
                        auto &px = view(x, y);

                        // Convert 8-bit coverage to [0..1]
                        float coverageF = coverage / 255.0f;

                        // Convert 8-bit userAlpha to [0..1]
                        float userAlphaF = userAlpha / 255.0f;

                        // Final alpha also in [0..1]
                        float alpha = coverageF * userAlphaF;
                        float invAlpha = 1.0f - alpha;

                        // Now alpha-blend correctly:
                        px[0] = static_cast<std::uint8_t>(alpha * color[0] + invAlpha * px[0]);
                        px[1] = static_cast<std::uint8_t>(alpha * color[1] + invAlpha * px[1]);
                        px[2] = static_cast<std::uint8_t>(alpha * color[2] + invAlpha * px[2]);
                        px[3] = static_cast<std::uint8_t>(alpha * 255.0f    + invAlpha * px[3]);

                    }
                }
            }
        }

        // Advance to the next glyph
        xOffset += (face->glyph->advance.x >> 6); // FreeType uses 26.6 fixed-point
    }
}

/// Compute the width and height (in pixels) of a single line of text using FreeType
/// returns {lineWidth, lineHeight}
std::pair<int,int> measure_text_line(FT_Face face, const std::string& textLine)
{
    int width = 0;
    int maxBearingY = 0;   // max distance from baseline to top of glyph
    int maxDescent = 0;    // how far glyph can go below baseline

    for (char c : textLine) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            continue; // skip on error
        }

        // Advance
        width += (face->glyph->advance.x >> 6);

        // Track bounding box info
        int bearingY = face->glyph->bitmap_top;
        if (bearingY > maxBearingY) {
            maxBearingY = bearingY;
        }
        int descent = face->glyph->bitmap.rows - bearingY;
        if (descent > maxDescent) {
            maxDescent = descent;
        }
    }

    int height = maxBearingY + maxDescent;
    return {width, height};
}
/// Given a block of text and a desired approximate square shape,
/// split the text into lines that approximate the same "target" line width
/// (based on total text length & font properties). This is a naïve method.
std::vector<std::string> wrap_text_to_square(FT_Face face, const std::string& text, int minWidth)
{
    // A very naive approach:
    // 1. Guess that we want ~ sqrt(length_of_text) characters per line
    // 2. Then do word-wrap so that no line exceeds that approximate # of characters in width (in pixels).

    // Might want to measure a character's approximate average width
    // We'll do something simpler: measure the average width for 'M' or something
    // for a baseline, or measure 'a' to 'z' etc. For simplicity:
    if (FT_Load_Char(face, 'M', FT_LOAD_RENDER) != 0) {
        throw std::runtime_error("Failed to load reference glyph 'M'.");
    }
    int averageCharWidth = (face->glyph->advance.x >> 6);

    // Rough guess:
    int textLen = static_cast<int>(text.size());
    int approxLineCharCount = static_cast<int>(std::sqrt((double)textLen));
    if (approxLineCharCount <= 0) approxLineCharCount = 1;

    // In pixels
    // clamp to the minWidth
    int maxLinePixelWidth = std::max(minWidth, approxLineCharCount * averageCharWidth);

    // Now, actual word-wrapping
    std::vector<std::string> lines;
    std::string currentLine;
    int currentLineWidth = 0;

    // Split text by spaces/newlines into words
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        // measure this word
        auto measure = measure_text_line(face, word + " "); // add a space for the next separation
        int wordWidth = measure.first;

        // check if adding this word would exceed maxLinePixelWidth
        if (!currentLine.empty() && (currentLineWidth + wordWidth > maxLinePixelWidth)) {
            // push current line
            lines.push_back(currentLine);
            // start a new one
            currentLine = word + " ";
            currentLineWidth = wordWidth;
        } else {
            currentLine += word + " ";
            currentLineWidth += wordWidth;
        }
    }
    // last line
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    return lines;
}
void Bgfx2DOverlayText_DynamicallySized::createImageAndSetupBgfxTexture(const std::string &text, const std::string &fontPath, int fontSize, gil::rgba8_pixel_t fontColor, int minWidth)
{
    {
        FT_Library ft;
        if (FT_Init_FreeType(&ft))
            throw std::runtime_error("Could not init freetype library");
        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
            throw std::runtime_error("Failed to load font");
        FT_Set_Pixel_Sizes(face, 0, fontSize);


        auto lines = wrap_text_to_square(face, text, minWidth);

        // 5. Measure each line to find total width & total height needed
        int maxWidth = 0;
        int totalHeight = 0;
        std::vector<LineMetrics> lineMetrics;
        for (auto &l : lines) {
            auto [w, h] = measure_text_line(face, l);
                    if (w > maxWidth) maxWidth = w;
                    lineMetrics.push_back({w, h, l});
            totalHeight += h;
        }

        // Add some margin:
        int margin = 5;
        int imageWidth = maxWidth + 2*margin;
        int imageHeight = totalHeight + 2*margin;

        // 6. Create a GIL RGBA8 image with transparent background
        image = gil::rgba8_image_t(imageWidth, imageHeight);
        auto view = gil::view(image);

        // Fill with transparent
        gil::rgba8_pixel_t transparent(0,0,0,0);
        for (int y = 0; y < imageHeight; ++y) {
            for (int x = 0; x < imageWidth; ++x) {
                view(x,y) = transparent;
            }
        }

        // 7. Render each line of text
        int yOffset = margin;
        for (auto &lm : lineMetrics)
        {
            // The baseline for the line is basically yOffset + lineAscent
            // For simplicity, we’ll assume the line’s top is yOffset + (its measured height).
            // A more robust approach is to measure each line’s ascent/descent individually.
            auto [lineWidth, lineHeight] = measure_text_line(face, lm.text);

                    // We want baseline at yOffset + the maxBearingY (within measure_text_line).
                    // But measure_text_line lumps that in.  So let's do a simplified approach:
                    // we call the baseline = yOffset + lineHeight - descent (some guess).

                    // Actually: measure_text_line gave us total height = maxBearingY + maxDescent.
                    // We can do baseline = yOffset + maxBearingY.

                    // But to keep it simpler, we’ll just place each line one after the other.
                    // Let’s do baseline at yOffset + lineHeight - some small portion.

                    // Actually let's do a quick hack:
                    int baselineY = yOffset + lineHeight * 3/4; // rough guess
                    draw_text_line(face, view, lm.text, margin, baselineY, fontColor);

                    yOffset += lineHeight;
        }
        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    } //maybe flush view

    const bgfx::Memory* mem = bgfx::makeRef(interleaved_view_get_raw_data(const_view(image)), image.width() * image.height() * 4);
    texture = bgfx::createTexture2D(image.width(), image.height(), false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
}
