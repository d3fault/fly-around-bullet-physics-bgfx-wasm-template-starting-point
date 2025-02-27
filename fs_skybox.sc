$input v_dir

#include <bgfx_shader.sh>

// Still using a 2D texture
SAMPLER2D(s_skybox, 0);

void main()
{
#if 1 //equirectangular spheroid thingy

    // Normalize the direction vector
    vec3 dir = normalize(v_dir);

    // Convert direction to spherical coordinates for equirectangular mapping
    float phi   = atan2(dir.z, dir.x);
    float theta = asin(dir.y);

    // Map spherical coordinates to UV range [0,1]
    float u = 0.5 + (phi / (2.0 * 3.14159265359));
    float v = 0.5 - (theta / 3.14159265359);

    // Sample the skybox texture
    vec4 color = texture2D(s_skybox, vec2(u, v));

    gl_FragColor = color;

#else //CUBE (not "cube map")

    // Normalize direction
    vec3 dir = normalize(v_dir);

    // Absolute direction values for cube face selection
    vec3 absDir = abs(dir);

    float ma;  // Major axis
    vec2 uv;   // UV for sampling

    if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
        // X-dominant: Left or Right face
        ma = absDir.x;
        uv = vec2((dir.z / ma + 1.0) * 0.5, (dir.y / ma + 1.0) * 0.5);
        if (dir.x > 0.0) uv.x = 1.0 - uv.x; // Right face
    }
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z) {
        // Y-dominant: Top or Bottom face
        ma = absDir.y;
        uv = vec2((dir.x / ma + 1.0) * 0.5, (dir.z / ma + 1.0) * 0.5);
        if (dir.y > 0.0) uv.y = 1.0 - uv.y; // Top face
    }
    else {
        // Z-dominant: Front or Back face
        ma = absDir.z;
        uv = vec2((dir.x / ma + 1.0) * 0.5, (dir.y / ma + 1.0) * 0.5);
        if (dir.z < 0.0) uv.x = 1.0 - uv.x; // Back face
    }

    // Convert cube UV to equirectangular UV mapping
    float u = uv.x;        // Already in range [0,1]
    float v = uv.y;        // Already in range [0,1]

    // Sample the original 2D skybox texture
    vec4 color = texture2D(s_skybox, vec2(u, v));

    gl_FragColor = color;
#endif
}
