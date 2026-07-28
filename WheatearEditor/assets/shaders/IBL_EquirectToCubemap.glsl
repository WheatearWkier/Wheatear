// =============================================================
//  IBL_EquirectToCubemap.glsl
//  Pass 1 of IBL precompute: equirectangular HDR -> cubemap
//
//  Binding layout:
//    binding = 0  u_EquirectMap (sampler2D, HDR equirect input)
//
//  Usage: render a unit cube with 6 face-view matrices,
//  capture into a GL_TEXTURE_CUBE_MAP target (e.g. 512x512).
// =============================================================

// -------------------------------------------------------------
//  VERTEX SHADER
// -------------------------------------------------------------
#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 1) uniform CaptureCamera
{
    mat4 u_ViewProjection;
};

layout(location = 0) out vec3 v_LocalPos;

void main()
{
    v_LocalPos  = a_Position;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}


// -------------------------------------------------------------
//  FRAGMENT SHADER
// -------------------------------------------------------------
#type fragment
#version 450 core

layout(location = 0) in  vec3 v_LocalPos;
layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_EquirectMap;

const vec2 INV_ATAN = vec2(0.1591, 0.3183); // (1/2pi, 1/pi)

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= INV_ATAN;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv    = SampleSphericalMap(normalize(v_LocalPos));
    vec3 color = texture(u_EquirectMap, uv).rgb;
    o_Color    = vec4(color, 1.0);
}
