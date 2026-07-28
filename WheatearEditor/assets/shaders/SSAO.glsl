// =============================================================================
//  SSAO.glsl
//
//  Binding 分配：
//    UBO  binding = 1   Camera       (BeginScene 每帧写入，直接复用)
//    UBO  binding = 15  SSAOParams   (radius / bias / power / noiseScale)
//    UBO  binding = 16  SSAOKernel   (64 个半球采样点，vec4 对齐)
//
//  Texture slot 分配：
//    slot 20  u_NormalTex  主 FBO attachment 1，视图空间法线 (RGBA16F)
//    slot 21  u_DepthTex   主 FBO depth attachment
//    slot 22  u_NoiseTex   4x4 随机旋转噪声 (RGBA8，REPEAT 平铺)
//
//  Vertex shader 用 gl_VertexID 生成全屏两个三角形，
//  与 EditorGrid shader 相同的无 VBO 方式，DrawArrays(QuadVAO, 6)。
// =============================================================================

// -----------------------------------------------------------------------------
//  VERTEX SHADER
// -----------------------------------------------------------------------------
#type vertex
#version 450 core

layout(location = 0) out vec2 v_TexCoord;

void main()
{
    // 6 个顶点 → 全屏两个三角形（不需要 VBO）
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

// Camera UBO（binding=1）—— BeginScene 已经上传，直接复用投影矩阵
layout(std140, binding = 1) uniform Camera
{
    mat4  u_ViewProjection;
    mat4  u_View;
    mat4  u_Projection;
    vec3  u_CameraPosition;
    float u_Near;
};

// SSAO 参数 UBO（binding=15）
// 与 C++ 端 SSAOParamsUBO 的 std140 布局必须完全一致
layout(std140, binding = 15) uniform SSAOParams
{
    vec4  u_NoiseScale;   // xy = (width/4, height/4)，zw 填充
    float u_Radius;
    float u_Bias;
    float u_Power;
    float _pad;
};

// SSAO 采样核 UBO（binding=16）
// std140 里 vec3 数组每个元素占 vec4 大小，所以直接用 vec4
layout(std140, binding = 16) uniform SSAOKernel
{
    vec4 u_Samples[64];   // xyz = 采样方向，w 未使用
};

// 纹理
layout(binding = 20) uniform sampler2D u_NormalTex;
layout(binding = 21) uniform sampler2D u_DepthTex;
layout(binding = 22) uniform sampler2D u_NoiseTex;

// 从深度缓冲重建视图空间位置
vec3 ReconstructViewPos(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = inverse(u_Projection) * clip;
    return view.xyz / view.w;
}

void main()
{
    float depth = texture(u_DepthTex, v_TexCoord).r;

    // 天空/空背景直接输出 AO=1（无遮蔽）
    if (depth >= 1.0)
    {
        o_AO = 1.0;
        return;
    }

    vec3 fragPos = ReconstructViewPos(v_TexCoord, depth);

    // 视图空间法线（PBR shader 已经输出到 attachment 1）
    vec3 normal = normalize(texture(u_NormalTex, v_TexCoord).rgb);

    // 用噪声纹理随机旋转 TBN，避免明显的条带瑕疵
    vec3 randVec   = normalize(texture(u_NoiseTex, v_TexCoord * u_NoiseScale.xy).xyz * 2.0 - 1.0);
    vec3 tangent   = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < 64; ++i)
    {
        // 视图空间采样位置
        vec3 samplePos = TBN * u_Samples[i].xyz;
        samplePos = fragPos + samplePos * u_Radius;

        // 投影到屏幕空间
        vec4 offset = u_Projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        // 采样该屏幕位置的实际深度，重建视图空间坐标
        float sampleDepth = texture(u_DepthTex, offset.xy).r;
        vec3  sampleView  = ReconstructViewPos(offset.xy, sampleDepth);

        // 范围检测衰减 + 遮蔽累积
        float rangeCheck = smoothstep(0.0, 1.0,
            u_Radius / abs(fragPos.z - sampleView.z));
        occlusion += (sampleView.z >= samplePos.z + u_Bias ? 1.0 : 0.0)
                     * rangeCheck;
    }

    o_AO = pow(1.0 - (occlusion / 64.0), u_Power);
}
