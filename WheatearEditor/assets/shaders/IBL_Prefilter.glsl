// =============================================================
//  IBL_Prefilter.glsl
//  Pass 3 of IBL precompute: cubemap -> specular pre-filter map
//
//  Binding layout:
//    binding = 0  u_EnvironmentMap (samplerCube)
//
//  Same unit-cube geometry as the other passes.
//  Call once per mip level (0..4), uploading u_Roughness each time.
//  Recommended resolution: base 128x128, 5 mip levels (128→8).
//
//  Uses GGX importance sampling with the Hammersley sequence.
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

layout(binding = 0) uniform samplerCube u_EnvironmentMap;

// Uploaded via push constant / UBO before each mip draw call
layout(std140, binding = 2) uniform PrefilterParams
{
    float u_Roughness;
    float u_Resolution; // face resolution of the source cubemap
};

const float PI       = 3.14159265359;
const uint  SAMPLES  = 1024u;

// -------------------------------------------------------
// Van der Corput radical inverse (Hammersley sequence)
// -------------------------------------------------------
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// -------------------------------------------------------
// GGX importance sample — returns a half-vector in world space
// -------------------------------------------------------
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a  = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Tangent-space half vector
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    // Tangent frame
    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);

    return normalize(right * H.x + up * H.y + N * H.z);
}

// -------------------------------------------------------
// GGX normal distribution (for solid-angle weighting)
// -------------------------------------------------------
float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

void main()
{
    vec3 N = normalize(v_LocalPos);
    // For the split-sum approximation we assume V == R == N
    // (introduces a small error at grazing angles, acceptable trade-off)
    vec3 V = N;

    float totalWeight = 0.0;
    vec3  prefilteredColor = vec3(0.0);

    for (uint i = 0u; i < SAMPLES; i++)
    {
        vec2 Xi = Hammersley(i, SAMPLES);
        vec3 H  = ImportanceSampleGGX(Xi, N, u_Roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            // Mip-level bias to reduce bright dots at high roughness
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float D     = D_GGX(NdotH, u_Roughness);
            float pdf   = (D * NdotH / (4.0 * HdotV)) + 0.0001;

            float saTexel  = 4.0 * PI / (6.0 * u_Resolution * u_Resolution);
            float saSample = 1.0 / (float(SAMPLES) * pdf + 0.0001);
            float mipLevel = (u_Roughness == 0.0)
                           ? 0.0
                           : 0.5 * log2(saSample / saTexel);

            prefilteredColor += textureLod(u_EnvironmentMap, L, mipLevel).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;
    o_Color = vec4(prefilteredColor, 1.0);
}
