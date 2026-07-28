// =============================================================
//  IBL_BrdfLUT.glsl
//  Pass 4 of IBL precompute: generate the BRDF integration LUT
//
//  Output: RG16F 2D texture, 512x512
//    R = scale (F0 contribution)
//    G = bias  (F90 / Fresnel remainder contribution)
//  UV axes:
//    U = NdotV  (0→1)
//    V = roughness (0→1)
//
//  Render a fullscreen quad (no input textures needed).
//  Result is view-independent — compute once, cache forever.
// =============================================================

// -------------------------------------------------------------
//  VERTEX SHADER — fullscreen triangle trick
// -------------------------------------------------------------
#type vertex
#version 450 core

layout(location = 0) out vec2 v_TexCoord;

void main()
{
    // Emit a clip-space triangle that covers the entire screen
    // using only gl_VertexID (no VBO required).
    vec2 pos = vec2((gl_VertexID & 1) * 2.0,
                    (gl_VertexID >> 1) * 2.0);
    v_TexCoord  = pos;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}


// -------------------------------------------------------------
//  FRAGMENT SHADER
// -------------------------------------------------------------
#type fragment
#version 450 core

layout(location = 0) in  vec2 v_TexCoord;
layout(location = 0) out vec2 o_Color; // RG16F: (scale, bias)

const float PI      = 3.14159265359;
const uint  SAMPLES = 1024u;

// Van der Corput radical inverse
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a  = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3  H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3  up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3  right = normalize(cross(up, N));
    up = cross(N, right);
    return normalize(right * H.x + up * H.y + N * H.z);
}

// Geometry function for BRDF LUT — uses k = roughness^2 / 2
// (IBL variant, different from direct lighting)
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith_IBL(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX_IBL(NdotV, roughness)
         * GeometrySchlickGGX_IBL(NdotL, roughness);
}

vec2 IntegrateBRDF(float NdotV, float roughness)
{
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float scale = 0.0;
    float bias  = 0.0;

    for (uint i = 0u; i < SAMPLES; i++)
    {
        vec2  Xi = Hammersley(i, SAMPLES);
        vec3  H  = ImportanceSampleGGX(Xi, N, roughness);
        vec3  L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z,          0.0);
        float NdotH = max(H.z,          0.0);
        float VdotH = max(dot(V, H),    0.0);

        if (NdotL > 0.0)
        {
            float G    = GeometrySmith_IBL(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc   = pow(1.0 - VdotH, 5.0);

            scale += (1.0 - Fc) * G_Vis;
            bias  +=        Fc  * G_Vis;
        }
    }

    return vec2(scale, bias) / float(SAMPLES);
}

void main()
{
    o_Color = IntegrateBRDF(v_TexCoord.x, v_TexCoord.y);
}
