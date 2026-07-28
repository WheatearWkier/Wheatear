#type vertex
#version 450 core

// Camera UBO binding=1（208 bytes）
layout(std140, binding = 1) uniform Camera
{
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec3 u_CameraPosition;
    float u_Near;
};

layout(location = 0) out vec3 v_NearPoint;
layout(location = 1) out vec3 v_FarPoint;

vec3 Unproject(float x, float y, float z, mat4 invVP)
{
    vec4 p = invVP * vec4(x, y, z, 1.0);
    return p.xyz / p.w;
}

void main()
{
    mat4 invVP = inverse(u_ViewProjection);

    float x = 0.0;
    float y = 0.0;

    if      (gl_VertexID == 0) { x = -1.0; y = -1.0; }
    else if (gl_VertexID == 1) { x =  1.0; y = -1.0; }
    else if (gl_VertexID == 2) { x =  1.0; y =  1.0; }
    else if (gl_VertexID == 3) { x = -1.0; y = -1.0; }
    else if (gl_VertexID == 4) { x =  1.0; y =  1.0; }
    else                          { x = -1.0; y =  1.0; }

    v_NearPoint = Unproject(x, y, 0.0, invVP);
    v_FarPoint  = Unproject(x, y, 1.0, invVP);
    gl_Position = vec4(x, y, 0.0, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) in vec3 v_NearPoint;
layout(location = 1) in vec3 v_FarPoint;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

layout(std140, binding = 1) uniform Camera
{
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec3 u_CameraPosition;
    float u_Near;
};

vec4 DrawGrid(vec3 pos, float scale)
{
    vec2 coord = pos.xz * scale;
    vec2 deriv = fwidth(coord);
    vec2 grid  = abs(fract(coord - 0.5) - 0.5) / deriv;
    float line = min(grid.x, grid.y);
    vec4 color = vec4(0.4, 0.4, 0.4, 1.0 - min(line, 1.0));

    // X 轴：红色
    if (abs(pos.x) < 0.06 / scale)
        color = vec4(0.85, 0.25, 0.25, color.a);

    // Z 轴：蓝色
    if (abs(pos.z) < 0.06 / scale)
        color = vec4(0.25, 0.35, 0.85, color.a);

    return color;
}

void main()
{
    // 光线与 Y=0 平面求交
    float t = -v_NearPoint.y / (v_FarPoint.y - v_NearPoint.y);
    if (t < 0.0) discard;

    vec3 worldPos = v_NearPoint + t * (v_FarPoint - v_NearPoint);

    // 手动写深度，让 grid 正确参与深度测试
    vec4 clipPos = u_ViewProjection * vec4(worldPos, 1.0);
    gl_FragDepth = (clipPos.z / clipPos.w) * 0.5 + 0.5;

    // 两层叠加：粗格（1 单位）+ 细格（0.1 单位）
    vec4 grid = DrawGrid(worldPos, 1.0) + DrawGrid(worldPos, 0.1);
    grid = clamp(grid, 0.0, 1.0);

    // 距离衰减
    float dist = length(u_CameraPosition.xz - worldPos.xz);
    float fade = 1.0 - smoothstep(30.0, 60.0, dist);
    grid.a    *= fade;

    if (grid.a < 0.005) discard;

    o_Color    = grid;
    o_EntityID = -1;
}