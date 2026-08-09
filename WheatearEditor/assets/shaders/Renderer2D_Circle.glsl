//--------------------------
// - Wheatear 2D -
// Renderer2D Circle Shader
// --------------------------

#type vertex
#version 450 core

layout(location = 0) in vec3 a_WorldPosition;
layout(location = 1) in vec3 a_LocalPosition;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Thickness;
layout(location = 4) in float a_Fade;
layout(location = 5) in float a_Progress;
layout(location = 6) in float a_StartAngle;
layout(location = 7) in int a_EntityID;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec3 LocalPosition;
	vec4 Color;
	float Thickness;
	float Fade;
	float Progress;
	float StartAngle;
};

layout (location = 0) out VertexOutput Output;
layout (location = 6) out flat int v_EntityID;

void main()
{
	Output.LocalPosition = a_LocalPosition;
	Output.Color = a_Color;
	Output.Thickness = a_Thickness;
	Output.Fade = a_Fade;
	Output.Progress = a_Progress;
	Output.StartAngle = a_StartAngle;

	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_WorldPosition, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput
{
	vec3 LocalPosition;
	vec4 Color;
	float Thickness;
	float Fade;
	float Progress;
	float StartAngle;
};

layout (location = 0) in VertexOutput Input;
layout (location = 6) in flat int v_EntityID;

void main()
{
    // Calculate distance and fill circle with white
    float distance = 1.0 - length(Input.LocalPosition);
    float circle = smoothstep(0.0, Input.Fade, distance);
    circle *= smoothstep(Input.Thickness + Input.Fade, Input.Thickness, distance);

	if (circle == 0.0)
		discard;

    if (Input.Progress >= 0.0)
    {
        if (Input.Progress >= 0.9999)
            discard;

        if (Input.Progress > 0.0001)
        {
            const float Tau = 6.28318530718;
            float angle = atan(Input.LocalPosition.y, Input.LocalPosition.x);
            float clockwiseSweep = mod(Input.StartAngle - angle + Tau, Tau);
            float revealedSweep = Input.Progress * Tau;
            if (clockwiseSweep < revealedSweep)
                discard;
        }
    }

    // Set output color
    o_Color = Input.Color;
	o_Color.a *= circle;

	o_EntityID = v_EntityID;
}
