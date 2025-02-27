$input a_position
$output v_dir

#include <bgfx_shader.sh>

uniform mat4 u_viewProjNoTrans;

void main()
{
    v_dir = a_position.xyz;
    gl_Position = mul(u_viewProjNoTrans, vec4(a_position, 1.0));
}

