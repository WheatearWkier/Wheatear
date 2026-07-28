// =============================================================
//  IBL_Irradiance.glsl
//  Pass 2 of IBL precompute: cubemap -> diffuse irradiance map
//
//  Binding layout:
//    binding = 0  u_EnvironmentMap (samplerCube)
//
//  Render a unit cube the same way as IBL_EquirectToCubemap,
//  capturing into a smaller cubemap (e.g. 32x32 is enough).
//  The convolution integral runs in the fragment shader.
// =============================================================

// -------------------------------------------------------------
//  VERTEX SHADER  (shared with EquirectToCubemap)
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

const float PI = 3.14159265359;

void main()
{
    // The world-space direction this fragment represents on the cubemap face
    vec3 N = normalize(v_LocalPos);

    // Build a tangent frame around N
    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);

    vec3  irradiance = vec3(0.0);
    float sampleDelta = 0.025; // ~5000 samples; raise to 0.05 if too slow
    float nrSamples   = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // Spherical -> tangent-space -> world
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta));
            vec3 sampleVec = tangentSample.x * right
                           + tangentSample.y * up
                           + tangentSample.z * N;

            irradiance += texture(u_EnvironmentMap, sampleVec).rgb
                        * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0 / nrSamples);
    o_Color    = vec4(irradiance, 1.0);
}
