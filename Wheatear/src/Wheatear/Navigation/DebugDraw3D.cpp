#include "wtpch.h"
#include "DebugDraw3D.h"

#include "Wheatear/Renderer/RenderCommand.h"
#include "Wheatear/Renderer/Shader.h"
#include "Wheatear/Renderer/VertexArray.h"

#include <vector>

namespace Wheatear {

    namespace {

        constexpr size_t kMaxVertices = 262144;

        struct DebugVertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
        };

        std::vector<DebugVertex> s_Vertices;
        Ref<VertexArray> s_VertexArray;
        Ref<VertexBuffer> s_VertexBuffer;
        Ref<Shader> s_Shader;

        const char* kVertexSource = R"(
#version 450 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 1) uniform Camera
{
    mat4 u_ViewProjection;
};

layout(location = 0) out vec4 v_Color;
void main()
{
    v_Color = a_Color;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

        const char* kFragmentSource = R"(
#version 450 core
layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;
void main()
{
    o_Color = v_Color;
}
)";

        void EnsureResources()
        {
            if (s_Shader)
                return;

            s_Shader = Shader::Create("DebugDraw3D", kVertexSource, kFragmentSource);
            s_VertexArray = VertexArray::Create();
            s_VertexBuffer = VertexBuffer::Create(
                static_cast<uint32_t>(sizeof(DebugVertex) * kMaxVertices));
            s_VertexBuffer->SetLayout({
                { "a_Position", ShaderDataType::Float3 },
                { "a_Color",    ShaderDataType::Float4 },
                });
            s_VertexArray->AddVertexBuffer(s_VertexBuffer);
        }

    } // namespace

    void DebugDraw3D::Begin()
    {
        s_Vertices.clear();
    }

    void DebugDraw3D::Line(const glm::vec3& from, const glm::vec3& to,
        const glm::vec4& color)
    {
        if (s_Vertices.size() + 2 > kMaxVertices)
            return;
        s_Vertices.push_back({ from, color });
        s_Vertices.push_back({ to, color });
    }

    void DebugDraw3D::Box(const glm::vec3& minimum, const glm::vec3& maximum,
        const glm::vec4& color)
    {
        const glm::vec3 corners[8] = {
            { minimum.x, minimum.y, minimum.z },
            { maximum.x, minimum.y, minimum.z },
            { maximum.x, minimum.y, maximum.z },
            { minimum.x, minimum.y, maximum.z },
            { minimum.x, maximum.y, minimum.z },
            { maximum.x, maximum.y, minimum.z },
            { maximum.x, maximum.y, maximum.z },
            { minimum.x, maximum.y, maximum.z },
        };

        const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},
            {4,5},{5,6},{6,7},{7,4},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (const auto& edge : edges)
            Line(corners[edge[0]], corners[edge[1]], color);
    }

    bool DebugDraw3D::HasLines()
    {
        return !s_Vertices.empty();
    }

    void DebugDraw3D::Flush()
    {
        if (s_Vertices.empty())
            return;

        EnsureResources();
        const uint32_t bytes = static_cast<uint32_t>(
            s_Vertices.size() * sizeof(DebugVertex));
        s_VertexBuffer->SetData(s_Vertices.data(), bytes);

        s_Shader->Bind();
        RenderCommand::DrawLines(s_VertexArray,
            static_cast<uint32_t>(s_Vertices.size()));
        s_Vertices.clear();
    }

} // namespace Wheatear
