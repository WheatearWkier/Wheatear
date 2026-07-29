// High-resolution alpha atlas text shader with screen-space outline sampling.

#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in vec4 a_OutlineColor;
layout(location = 5) in float a_OutlineWidth;
layout(location = 6) in float a_EdgeSoftness;
layout(location = 7) in int a_EntityID;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
	vec4 OutlineColor;
	float OutlineWidth;
	float EdgeSoftness;
};

layout(location = 0) out VertexOutput Output;
layout(location = 5) out flat float v_TexIndex;
layout(location = 6) out flat int v_EntityID;

void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoord;
	Output.OutlineColor = a_OutlineColor;
	Output.OutlineWidth = a_OutlineWidth;
	Output.EdgeSoftness = a_EdgeSoftness;
	v_TexIndex = a_TexIndex;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 color;
layout(location = 1) out int color2;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
	vec4 OutlineColor;
	float OutlineWidth;
	float EdgeSoftness;
};

layout(location = 0) in VertexOutput Input;
layout(location = 5) in flat float v_TexIndex;
layout(location = 6) in flat int v_EntityID;

layout(binding = 0) uniform sampler2D u_Textures[32];

vec4 SampleTextAtlas(int index, vec2 uv)
{
	switch(index)
	{
		case  0: return texture(u_Textures[ 0], uv);
		case  1: return texture(u_Textures[ 1], uv);
		case  2: return texture(u_Textures[ 2], uv);
		case  3: return texture(u_Textures[ 3], uv);
		case  4: return texture(u_Textures[ 4], uv);
		case  5: return texture(u_Textures[ 5], uv);
		case  6: return texture(u_Textures[ 6], uv);
		case  7: return texture(u_Textures[ 7], uv);
		case  8: return texture(u_Textures[ 8], uv);
		case  9: return texture(u_Textures[ 9], uv);
		case 10: return texture(u_Textures[10], uv);
		case 11: return texture(u_Textures[11], uv);
		case 12: return texture(u_Textures[12], uv);
		case 13: return texture(u_Textures[13], uv);
		case 14: return texture(u_Textures[14], uv);
		case 15: return texture(u_Textures[15], uv);
		case 16: return texture(u_Textures[16], uv);
		case 17: return texture(u_Textures[17], uv);
		case 18: return texture(u_Textures[18], uv);
		case 19: return texture(u_Textures[19], uv);
		case 20: return texture(u_Textures[20], uv);
		case 21: return texture(u_Textures[21], uv);
		case 22: return texture(u_Textures[22], uv);
		case 23: return texture(u_Textures[23], uv);
		case 24: return texture(u_Textures[24], uv);
		case 25: return texture(u_Textures[25], uv);
		case 26: return texture(u_Textures[26], uv);
		case 27: return texture(u_Textures[27], uv);
		case 28: return texture(u_Textures[28], uv);
		case 29: return texture(u_Textures[29], uv);
		case 30: return texture(u_Textures[30], uv);
		case 31: return texture(u_Textures[31], uv);
	}
	return vec4(1.0, 1.0, 1.0, 0.0);
}

float SampleCoverage(int index, vec2 uv)
{
	return SampleTextAtlas(index, uv).a;
}

void main()
{
	int textureIndex = int(v_TexIndex);
	vec2 uv = Input.TexCoord;
	float glyphAlpha = SampleCoverage(textureIndex, uv);

	float fillCoverage = glyphAlpha;
	float outlineCoverage = fillCoverage;

	if (Input.OutlineWidth > 0.001 && Input.OutlineColor.a > 0.001)
	{
		vec2 dx = dFdx(uv) * Input.OutlineWidth;
		vec2 dy = dFdy(uv) * Input.OutlineWidth;
		float neighborAlpha = glyphAlpha;
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv + dx));
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv - dx));
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv + dy));
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv - dy));
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv + dx + dy));
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv + dx - dy));
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv - dx + dy));
		neighborAlpha = max(neighborAlpha, SampleCoverage(textureIndex, uv - dx - dy));
		outlineCoverage = neighborAlpha;
	}

	float fillAlpha = fillCoverage * Input.Color.a;
	float outlineAlpha = outlineCoverage * Input.OutlineColor.a;
	float outputAlpha = max(fillAlpha, outlineAlpha);
	if (outputAlpha <= 0.001)
		discard;

	vec3 outputRGB = Input.OutlineColor.rgb;
	if (fillAlpha > 0.001)
		outputRGB = mix(Input.OutlineColor.rgb, Input.Color.rgb, clamp(fillAlpha / max(outputAlpha, 0.001), 0.0, 1.0));

	color = vec4(outputRGB, outputAlpha);
	color2 = v_EntityID;
}
