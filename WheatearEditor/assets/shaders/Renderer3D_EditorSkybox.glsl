// =============================================================
//  Renderer3D_EditorSkybox.glsl
//  Renders the editor skybox using the IBL environment cubemap.
//
//  When u_HasIBL == 0 (no HDR loaded), falls back to the
//  hemisphere gradient (3 colour gradient, same as before).
//
//  Binding layout:
//    binding = 1   Camera UBO   (shared, already uploaded)
//    binding = 10  IBLData UBO  (u_HasIBL, u_IBLIntensity)
//    binding = 12  u_PrefilterMap (samplerCube, mip 0 = environment)
//    binding = 14  SkyboxGradient UBO  (fallback colours)
//
//  Note: binding 14 is new — it replaces the old binding 10
//  Skybox UBO which held the three gradient colours.
//  The C++ side uploads SkyboxGradientUBOData to binding 14.
// =============================================================

// -------------------------------------------------------------
//  VERTEX SHADER
// -------------------------------------------------------------
#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 1) uniform Camera
{
    mat4  u_ViewProjection;
    mat4  u_View;
    mat4  u_Projection;
    vec3  u_CameraPosition;
    float u_Near;
};

layout(location = 0) out vec3 v_LocalPos;

void main()
{
    v_LocalPos = a_Position;

    // Remove translation from view matrix so the skybox stays centred
    mat4 rotView = mat4(mat3(u_View));
    vec4 clipPos = u_Projection * rotView * vec4(a_Position, 1.0);

    // Set z = w so the skybox always lands on the far plane (depth = 1.0)
    gl_Position = clipPos.xyww;
}


// -------------------------------------------------------------
//  FRAGMENT SHADER
// -------------------------------------------------------------
#type fragment
#version 450 core

layout(location = 0) in  vec3 v_LocalPos;
layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

layout(std140, binding = 10) uniform IBLData
{
    float u_IBLIntensity;
    int   u_HasIBL;
    float _ibl_pad[2];
};

layout(std140, binding = 14) uniform SkyboxGradient
{
    vec4 u_TopColor;
    vec4 u_HorizonColor;
    vec4 u_BottomColor;
};

// Mip 0 of the prefilter map == the full-resolution environment
layout(binding = 12) uniform samplerCube u_PrefilterMap;

void main()
{
    if (u_HasIBL != 0)
    {
        // Sample the environment map at mip 0 (sharpest = sky appearance)
        vec3 envColor = textureLod(u_PrefilterMap, normalize(v_LocalPos), 0.0).rgb;

        // Reinhard tone-map + gamma for display (IBL maps are linear HDR)
        envColor = envColor / (envColor + vec3(1.0));
        envColor = pow(envColor, vec3(1.0 / 2.2));

        o_Color = vec4(envColor, 1.0);
    }
    //else
    //{
        //// Hemisphere gradient fallback
        //float t = normalize(v_LocalPos).y; // -1..1

        //vec3 color;
        //if (t >= 0.0)
            //color = mix(u_HorizonColor.rgb, u_TopColor.rgb, t);
        //else
            //color = mix(u_HorizonColor.rgb, u_BottomColor.rgb, -t);

        //o_Color = vec4(color, 1.0);
    //}
    else
    {
        // 获取单位化的 Y 轴高度 (-1.0 到 1.0)
        float y = normalize(v_LocalPos).y;
        
        // 加入重映射或者平滑函数（比如使用 pow 或 smoothstep）
        // 这样做可以让 Y 从 0 到 1 的渐变曲线在前方视界变得非常平缓，扩大地平线过度带
        float k_top = max(y, 0.0);
        k_top = pow(k_top, 0.5); // 0.5 次方可以大幅拉宽渐变带，让蓝色不会一抬手就封顶
        
        float k_bottom = max(-y, 0.0);
        k_bottom = pow(k_bottom, 0.5); // 同样让下方的棕色过渡更柔和

        vec3 envColor = mix(u_HorizonColor.rgb, u_TopColor.rgb, k_top);
        envColor = mix(envColor, u_BottomColor.rgb, k_bottom);
        
        o_Color = vec4(envColor, 1.0);
    }

    o_EntityID = -1;
}
