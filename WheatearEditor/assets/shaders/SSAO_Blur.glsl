// =============================================================================
//  SSAO_Blur.glsl
//
//  对 SSAO 原始结果做 5x5 box blur 去噪。
//  Texture slot 20 复用（和 SSAO pass 的 u_NormalTex slot 相同，
//  但这个 pass 绑定的是 SSAO 原始 FBO 的输出）。
// =============================================================================

// -----------------------------------------------------------------------------
//  VERTEX SHADER
// -----------------------------------------------------------------------------
#type vertex
#version 450 core

layout(location = 0) out vec2 v_TexCoord;

void main()
{
    float x = 0.0;
    float y = 0.0;

    if      (gl_VertexID == 0) { x = -1.0; y = -1.0; }
    else if (gl_VertexID == 1) { x =  1.0; y = -1.0; }
    else if (gl_VertexID == 2) { x =  1.0; y =  1.0; }
    else if (gl_VertexID == 3) { x = -1.0; y = -1.0; }
    else if (gl_VertexID == 4) { x =  1.0; y =  1.0; }
    else                        { x = -1.0; y =  1.0; }

    v_TexCoord  = vec2(x, y) * 0.5 + 0.5;
    gl_Position = vec4(x, y, 0.0, 1.0);
}

// -----------------------------------------------------------------------------
//  FRAGMENT SHADER
// -----------------------------------------------------------------------------
#type fragment
#version 450 core

layout(location = 0) out float o_AO;

layout(location = 0) in vec2 v_TexCoord;

layout(binding = 20) uniform sampler2D u_AOTex;

void main()
{
    vec2  texelSize = 1.0 / vec2(textureSize(u_AOTex, 0));
    float result    = 0.0;

    // 5x5 box blur，共 25 个采样
    for (int x = -2; x <= 2; ++x)
    for (int y = -2; y <= 2; ++y)
        result += texture(u_AOTex, v_TexCoord + vec2(x, y) * texelSize).r;

    o_AO = result / 25.0;
}
