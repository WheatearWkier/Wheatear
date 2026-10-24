#type vertex
#version 450 core
layout(location = 0) in vec3 a_Position;
layout(location = 3) in ivec4 a_BoneIndices;
layout(location = 4) in vec4 a_BoneWeights;

layout(std140, binding = 2) uniform Transform
{
    mat4 u_Model;
    mat4 u_NormalMatrix;
    mat4 u_LightSpaceMatrix;
    int  u_Instanced;
    int  _instance_pad[3];
};

layout(location = 5) in vec4 a_InstanceModel0;
layout(location = 6) in vec4 a_InstanceModel1;
layout(location = 7) in vec4 a_InstanceModel2;
layout(location = 8) in vec4 a_InstanceModel3;

layout(std140, binding = 15) uniform BoneData
{
    mat4 u_Bones[256];
    int  u_Skinned;
    int  _bone_pad[3];
};

void main()
{
    vec4 localPos = vec4(a_Position, 1.0);
    if (u_Skinned != 0)
    {
        mat4 skin =
            u_Bones[a_BoneIndices.x] * a_BoneWeights.x +
            u_Bones[a_BoneIndices.y] * a_BoneWeights.y +
            u_Bones[a_BoneIndices.z] * a_BoneWeights.z +
            u_Bones[a_BoneIndices.w] * a_BoneWeights.w;
        localPos = skin * localPos;
    }
    mat4 instanceModel = mat4(1.0);
    if (u_Instanced != 0)
    {
        instanceModel = mat4(a_InstanceModel0, a_InstanceModel1,
                             a_InstanceModel2, a_InstanceModel3);
    }
    gl_Position = u_LightSpaceMatrix * u_Model * instanceModel * localPos;
}

#type fragment
#version 450 core
void main() {}