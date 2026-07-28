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

void main()
{
    vec4 worldPos   = u_Model * vec4(a_Position, 1.0);
    v_WorldPos      = worldPos.xyz;
    v_Normal        = normalize(mat3(u_NormalMatrix) * a_Normal);
    v_TexCoord      = a_TexCoord;
    v_LightSpacePos = u_LightSpaceMatrix * worldPos;
    gl_Position     = u_ViewProjection * worldPos;
}


#type fragment
#version 450 core

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec4 v_LightSpacePos;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

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

layout(binding = 4) uniform sampler2D u_AlbedoMap;
layout(binding = 6) uniform sampler2D u_NormalMap;
layout(binding = 7) uniform sampler2D u_RoughnessMap;
layout(binding = 8) uniform sampler2D u_MetallicMap;
layout(binding = 9) uniform sampler2D u_ShadowMap;

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

layout(std140, binding = 10) uniform Skybox
{
    vec4 u_TopColor;
    vec4 u_HorizonColor;
    vec4 u_BottomColor;
};

// ---- Shadow ----

//const float k_AmbientStrength = 0.08;

// PCF soft shadow, 3x3 samples.
float CalcShadow(vec4 lightSpacePos, vec3 N, vec3 L)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.0005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_ShadowMap, 0);
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = texture(u_ShadowMap,
                projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

// ---- Lighting ----

vec3 CalcDirectionalLight(DirectionalLight light, vec3 N, vec3 V,
                          vec3 albedo, float roughness, float shadow)
{
    float intensity = light.Color.w;
    if (intensity < 0.0001) return vec3(0.0);

    vec3 dir = light.Direction.xyz;
    if (length(dir) < 0.0001) return vec3(0.0);

    vec3  L         = normalize(-dir);
    vec3  H         = normalize(L + V);
    vec3  color     = light.Color.rgb * intensity;
    float diff      = max(dot(N, L), 0.0);
    float shininess = mix(128.0, 2.0, roughness);
    float spec      = pow(max(dot(N, H), 0.0), shininess) * (1.0 - roughness);

    return (1.0 - shadow) * (diff + spec) * color * albedo;
}

vec3 CalcPointLight(PointLight light, vec3 N, vec3 V,
                    vec3 worldPos, vec3 albedo, float roughness)
{
    vec3  L         = normalize(light.Position.xyz - worldPos);
    vec3  H         = normalize(L + V);
    float intensity = light.Color.w;
    vec3  color     = light.Color.rgb * intensity;
    float diff      = max(dot(N, L), 0.0);
    float shininess = mix(128.0, 2.0, roughness);
    float spec      = pow(max(dot(N, H), 0.0), shininess) * (1.0 - roughness);
    float d         = length(light.Position.xyz - worldPos);
    float att       = 1.0 / (light.Constant + light.Linear * d + light.Quadratic * d * d);

    return (diff + spec) * color * albedo * att;
}

void main()
{
    vec4 albedoSample = texture(u_AlbedoMap, v_TexCoord) * u_Albedo;
    vec3 albedo       = albedoSample.rgb;

    vec3 N = normalize(v_Normal);
    if (u_FlipNormals != 0) N = -N;

    if (u_HasNormalMap != 0)
    {
        vec3 T = normalize(dFdx(v_WorldPos) * (dFdy(v_TexCoord).t)
                         - dFdy(v_WorldPos) * (dFdx(v_TexCoord).t));
        T = normalize(T - dot(T, N) * N);
        vec3 B   = cross(N, T);
        mat3 TBN = mat3(T, B, N);

        vec3 normalSample = texture(u_NormalMap, v_TexCoord).rgb * 2.0 - 1.0;
        N = normalize(TBN * normalSample);
    }

    vec3  V         = normalize(u_CameraPosition - v_WorldPos);
    float roughness = (u_HasRoughnessMap != 0)
                    ? texture(u_RoughnessMap, v_TexCoord).r : u_Roughness;
    float metallic  = (u_HasMetallicMap != 0)
                    ? texture(u_MetallicMap,  v_TexCoord).r : u_Metallic;

    vec3  dirL  = normalize(-u_DirLight.Direction.xyz);
    float shadow = (u_HasShadowMap != 0)
                 ? CalcShadow(v_LightSpacePos, N, dirL)
                 : 0.0;

    // 在 fragment shader 里替换掉固定的 k_AmbientStrength
    vec3 ambientUp   = u_TopColor.rgb * 0.3;
    vec3 ambientDown = u_BottomColor.rgb * 0.1;
    float upFactor   = N.y * 0.5 + 0.5; // 把 [-1,1] 映射到 [0,1]
    vec3 ambient     = mix(ambientDown, ambientUp, upFactor);
    vec3 result      = ambient * albedo;
    
    //vec3 result = k_AmbientStrength * albedo;
    result += CalcDirectionalLight(u_DirLight, N, V, albedo, roughness, shadow);

    for (int i = 0; i < u_ActivePointLights; i++)
        result += CalcPointLight(u_PointLights[i], N, V, v_WorldPos, albedo, roughness);

    o_Color    = vec4(result, albedoSample.a);
    o_EntityID = u_EntityID;
}
