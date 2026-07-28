// =============================================================
//  Renderer3D_Mesh_PBR.glsl  -  PBR / Cook-Torrance BRDF
//  Direct lighting  +  Image-Based Lighting (IBL)  +  SSAO
//
//  相比原版的改动：
//    Vertex:
//      + layout(location=4) out vec3 v_ViewNormal  (视图空间法线)
//      + v_ViewNormal = normalize(mat3(u_View) * v_Normal)
//    Fragment:
//      + layout(location=1) out vec3 o_ViewNormal  (输出到主FBO attachment 1)
//      + layout(location=2) out int  o_EntityID    (原来是 location=1，现在移到 2)
//      + IBLData UBO 增加 HasSSAO / _ibl_pad 改为 float _ibl_pad[1]
//      + layout(binding=14) uniform sampler2D u_SSAOTex
//      + main() 里 ambient 乘以 AO 值
// =============================================================

// -------------------------------------------------------------
//  VERTEX SHADER
// -------------------------------------------------------------
#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(std140, binding = 1) uniform Camera
{
    mat4  u_ViewProjection;
    mat4  u_View;
    mat4  u_Projection;
    vec3  u_CameraPosition;
    float u_Near;
};

layout(std140, binding = 2) uniform Transform
{
    mat4 u_Model;
    mat4 u_NormalMatrix;
    mat4 u_LightSpaceMatrix;
};

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out vec4 v_LightSpacePos;
layout(location = 4) out vec3 v_ViewNormal;    // 新增：视图空间法线

void main()
{
    vec4 worldPos    = u_Model * vec4(a_Position, 1.0);
    v_WorldPos       = worldPos.xyz;
    v_Normal         = normalize(mat3(u_NormalMatrix) * a_Normal);
    v_TexCoord       = a_TexCoord;
    v_LightSpacePos  = u_LightSpaceMatrix * worldPos;
    v_ViewNormal     = normalize(mat3(u_View) * v_Normal);  // 新增
    gl_Position      = u_ViewProjection * worldPos;
}


// -------------------------------------------------------------
//  FRAGMENT SHADER
// -------------------------------------------------------------
#type fragment
#version 450 core

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec4 v_LightSpacePos;
layout(location = 4) in vec3 v_ViewNormal;     // 新增

// 主 FBO 颜色输出：
//   attachment 0  RGBA8   最终颜色（不变）
//   attachment 1  RGBA16F 视图空间法线（新增）
//   attachment 2  R32I    entity ID（原来 location=1，现在改为 2）
layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec3 o_ViewNormal;    // 新增
layout(location = 2) out int  o_EntityID;      // 从 location=1 改为 2

// ---------- UBOs ----------

layout(std140, binding = 1) uniform Camera
{
    mat4  u_ViewProjection;
    mat4  u_View;
    mat4  u_Projection;
    vec3  u_CameraPosition;
    float u_Near;
};

layout(std140, binding = 3) uniform Material
{
    vec4  u_Albedo;
    float u_Metallic;
    float u_Roughness;
    int   u_EntityID;
    int   u_FlipNormals;
    int   u_HasNormalMap;
    int   u_HasRoughnessMap;
    int   u_HasMetallicMap;
    int   u_HasShadowMap;
};

struct DirectionalLight
{
    vec4 Direction; // w unused
    vec4 Color;     // w = intensity
};

struct PointLight
{
    vec4  Position;  // w unused
    vec4  Color;     // w = intensity
    float Constant;
    float Linear;
    float Quadratic;
    float _pad;
};

layout(std140, binding = 5) uniform LightData
{
    DirectionalLight u_DirLight;
    PointLight       u_PointLights[8];
    int              u_ActivePointLights;
    float            _light_pad[3];
};

// IBLData UBO：增加 HasSSAO 字段（_ibl_pad 从 float[2] 缩减为 float[1]）
// 注意：C++ 端的 IBLDataUBO struct 也要同步修改
layout(std140, binding = 10) uniform IBLData
{
    float u_IBLIntensity;
    int   u_HasIBL;
    int   u_HasSSAO;    // 新增
    float _ibl_pad;     // 原来 _ibl_pad[2]，现在只剩 1 个
};

// ---------- Textures ----------

layout(binding = 4)  uniform sampler2D   u_AlbedoMap;
layout(binding = 6)  uniform sampler2D   u_NormalMap;
layout(binding = 7)  uniform sampler2D   u_RoughnessMap;
layout(binding = 8)  uniform sampler2D   u_MetallicMap;
layout(binding = 9)  uniform sampler2D   u_ShadowMap;
layout(binding = 11) uniform samplerCube u_IrradianceMap;
layout(binding = 12) uniform samplerCube u_PrefilterMap;
layout(binding = 13) uniform sampler2D   u_BrdfLUT;
layout(binding = 14) uniform sampler2D   u_SSAOTex;     // 新增：模糊后的 AO 结果

// =============================================================
//  PBR helpers（与原版完全相同，不改动）
// =============================================================

const float PI                = 3.14159265359;
const float MAX_PREFILTER_LOD = 4.0;

float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float G_SchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    return G_SchlickGGX(NdotV, roughness)
         * G_SchlickGGX(NdotL, roughness);
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
              * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// =============================================================
//  PCF Shadow（与原版相同）
// =============================================================
float CalcShadow(vec4 lightSpacePos, vec3 N, vec3 L)
{
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;
    float currentDepth = proj.z;
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = texture(u_ShadowMap, proj.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

// =============================================================
//  Direct BRDF（与原版相同）
// =============================================================
vec3 BRDF(vec3 N, vec3 V, vec3 L,
          vec3 albedo, float roughness, float metallic,
          vec3 F0, vec3 radiance)
{
    vec3  H     = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    if (NdotL < 0.0001) return vec3(0.0);

    float D   = D_GGX(NdotH, roughness);
    float G   = G_Smith(NdotV, NdotL, roughness);
    vec3  F   = F_Schlick(HdotV, F0);
    vec3  spec = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    vec3  kD   = (vec3(1.0) - F) * (1.0 - metallic);
    vec3  diff = kD * albedo / PI;

    return (diff + spec) * radiance * NdotL;
}

vec3 CalcDirLight(DirectionalLight light,
                  vec3 N, vec3 V,
                  vec3 albedo, float roughness, float metallic,
                  vec3 F0, float shadow)
{
    float intensity = light.Color.w;
    if (intensity < 0.0001) return vec3(0.0);
    vec3 dir = light.Direction.xyz;
    if (length(dir) < 0.0001) return vec3(0.0);
    vec3 L        = normalize(-dir);
    vec3 radiance = light.Color.rgb * intensity;
    return (1.0 - shadow) * BRDF(N, V, L, albedo, roughness, metallic, F0, radiance);
}

vec3 CalcPointLight(PointLight light,
                    vec3 N, vec3 V, vec3 worldPos,
                    vec3 albedo, float roughness, float metallic,
                    vec3 F0)
{
    vec3  toLight  = light.Position.xyz - worldPos;
    vec3  L        = normalize(toLight);
    float d        = length(toLight);
    float att      = 1.0 / (light.Constant
                           + light.Linear    * d
                           + light.Quadratic * d * d);
    vec3  radiance = light.Color.rgb * light.Color.w * att;
    return BRDF(N, V, L, albedo, roughness, metallic, F0, radiance);
}

// =============================================================
//  IBL（与原版相同）
// =============================================================
vec3 CalcIBL(vec3 N, vec3 V, vec3 R,
             vec3 albedo, float roughness, float metallic,
             vec3 F0)
{
    float NdotV = max(dot(N, V), 0.0);

    vec3 kS   = F_SchlickRoughness(NdotV, F0, roughness);
    vec3 kD   = (1.0 - kS) * (1.0 - metallic);
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuse    = kD * irradiance * albedo;

    float lod              = roughness * MAX_PREFILTER_LOD;
    vec3  prefilteredColor = textureLod(u_PrefilterMap, R, lod).rgb;
    vec2  brdf             = texture(u_BrdfLUT, vec2(NdotV, roughness)).rg;
    vec3  specular         = prefilteredColor * (kS * brdf.x + brdf.y);

    return (diffuse + specular) * u_IBLIntensity;
}

// =============================================================
//  Normal mapping（与原版相同）
// =============================================================
vec3 CalcNormal()
{
    vec3 N = normalize(v_Normal);
    if (u_FlipNormals != 0) N = -N;

    if (u_HasNormalMap != 0)
    {
        vec3 dp1  = dFdx(v_WorldPos);
        vec3 dp2  = dFdy(v_WorldPos);
        vec2 duv1 = dFdx(v_TexCoord);
        vec2 duv2 = dFdy(v_TexCoord);

        vec3 T = normalize(dp1 * duv2.t - dp2 * duv1.t);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);

        vec3 normalSample = texture(u_NormalMap, v_TexCoord).rgb * 2.0 - 1.0;
        N = normalize(mat3(T, B, N) * normalSample);
    }
    return N;
}

vec3 ToneMap(vec3 color)      { return color / (color + vec3(1.0)); }
vec3 GammaCorrect(vec3 color) { return pow(color, vec3(1.0 / 2.2)); }

// =============================================================
//  main
// =============================================================
void main()
{
    vec4 albedoSample = texture(u_AlbedoMap, v_TexCoord) * u_Albedo;
    vec3 albedo       = pow(albedoSample.rgb, vec3(2.2));

    float roughness = (u_HasRoughnessMap != 0)
                    ? texture(u_RoughnessMap, v_TexCoord).r
                    : u_Roughness;
    float metallic  = (u_HasMetallicMap != 0)
                    ? texture(u_MetallicMap, v_TexCoord).r
                    : u_Metallic;
    roughness = max(roughness, 0.04);

    vec3 N = CalcNormal();
    vec3 V = normalize(u_CameraPosition - v_WorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // 直接光
    vec3  dirL   = normalize(-u_DirLight.Direction.xyz);
    float shadow = (u_HasShadowMap != 0)
                 ? CalcShadow(v_LightSpacePos, N, dirL) : 0.0;

    vec3 Lo = vec3(0.0);
    Lo += CalcDirLight(u_DirLight, N, V, albedo, roughness, metallic, F0, shadow);
    for (int i = 0; i < u_ActivePointLights; i++)
        Lo += CalcPointLight(u_PointLights[i], N, V, v_WorldPos,
                             albedo, roughness, metallic, F0);

    // SSAO：用 gl_FragCoord 计算当前像素的屏幕 UV
    vec2  ssaoUV = gl_FragCoord.xy / vec2(textureSize(u_SSAOTex, 0));
    float ao     = (u_HasSSAO != 0) ? texture(u_SSAOTex, ssaoUV).r : 1.0;

    // 间接光 / Ambient（乘以 AO）
    vec3 ambient;
    if (u_HasIBL != 0)
    {
        ambient = CalcIBL(N, V, R, albedo, roughness, metallic, F0) * ao;
    }
    else
    {
        float upFactor = N.y * 0.5 + 0.5;
        ambient = mix(vec3(0.04), vec3(0.12), upFactor)
                * albedo
                * (1.0 - metallic * 0.85)
                * ao;
    }

    vec3 color = ambient + Lo;
    color = ToneMap(color);
    color = GammaCorrect(color);

    o_Color      = vec4(color, albedoSample.a);
    o_ViewNormal = normalize(v_ViewNormal);  // 新增：输出到 attachment 1
    o_EntityID   = u_EntityID;              // location 从 1 改为 2
}
