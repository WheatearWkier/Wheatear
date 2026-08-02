#include "wtpch.h"
#include "Renderer2D.h"
#include "RenderBatch.h"
#include "Renderer2DVertices.h"
#include "UniformBuffer.h"
#include "RenderCommand.h"
#include "Shader.h"
#include "Wheatear/Scene/Components.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Wheatear {


    static constexpr uint32_t k_MaxQuads = 10000;
    static constexpr uint32_t k_MaxVertices = k_MaxQuads * 4;
    static constexpr uint32_t k_MaxIndices = k_MaxQuads * 6;
    static constexpr uint32_t k_MaxTexSlots = 32;

    static constexpr glm::vec4 k_QuadPositions[4] = {
        { -0.5f, -0.5f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.0f, 1.0f },
        {  0.5f,  0.5f, 0.0f, 1.0f },
        { -0.5f,  0.5f, 0.0f, 1.0f },
    };

    static constexpr glm::vec2 k_DefaultUV[4] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f },
        { 1.0f, 1.0f }, { 0.0f, 1.0f },
    };

    //  Renderer2DData

    struct Renderer2DData
    {
        std::array<Ref<Texture2D>, k_MaxTexSlots> TextureSlots;
        uint32_t TextureSlotIndex = 1;
        Ref<Texture2D> WhiteTexture;

        Scope<RenderBatch<QuadVertex>>   QuadBatch;
        Scope<RenderBatch<CircleVertex>> CircleBatch;
        Scope<RenderBatch<LineVertex>>   LineBatch;
        Scope<RenderBatch<TextVertex>>   TextBatch;

        struct CameraData { glm::mat4 ViewProjection; };
        CameraData         CameraBuffer;
        Ref<UniformBuffer> CameraUniformBuffer;

        Renderer2D::Statistics Stats;
    };

    static Renderer2DData s_Data;


    void Renderer2D::Init()
    {
        WT_PROFILE_FUNCTION();

        auto* indices = new uint32_t[k_MaxIndices];
        for (uint32_t i = 0, offset = 0; i < k_MaxIndices; i += 6, offset += 4)
        {
            indices[i + 0] = offset + 0; indices[i + 1] = offset + 1; indices[i + 2] = offset + 2;
            indices[i + 3] = offset + 2; indices[i + 4] = offset + 3; indices[i + 5] = offset + 0;
        }
        Ref<IndexBuffer> sharedIBO = IndexBuffer::Create(indices, k_MaxIndices);
        delete[] indices;

        s_Data.WhiteTexture = Texture2D::Create(1, 1);
        constexpr uint32_t white = 0xffffffff;
        s_Data.WhiteTexture->SetData(const_cast<uint32_t*>(&white), sizeof(uint32_t));
        s_Data.TextureSlots[0] = s_Data.WhiteTexture;

        s_Data.QuadBatch = CreateScope<RenderBatch<QuadVertex>>(
            k_MaxVertices,
            Shader::Create("assets/shaders/Renderer2D_Quad.glsl"),
            BufferLayout{
                { "a_Position",     ShaderDataType::Float3 },
                { "a_Color",        ShaderDataType::Float4 },
                { "a_TexCoord",     ShaderDataType::Float2 },
                { "a_TexIndex",     ShaderDataType::Float  },
                { "a_TilingFactor", ShaderDataType::Float  },
                { "a_EntityID",     ShaderDataType::Int    }
            },
            BatchDrawMode::Triangles,
            sharedIBO
        );

        int samplers[k_MaxTexSlots];
        for (uint32_t i = 0; i < k_MaxTexSlots; ++i)
            samplers[i] = static_cast<int>(i);
        s_Data.QuadBatch->GetShader()->Bind();
        s_Data.QuadBatch->GetShader()->SetIntArray("u_Textures", samplers, k_MaxTexSlots);

        s_Data.QuadBatch->SetPreDrawCallback([]()
            {
                for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++)
                    s_Data.TextureSlots[i]->Bind(i);
            });

        s_Data.CircleBatch = CreateScope<RenderBatch<CircleVertex>>(
            k_MaxVertices,
            Shader::Create("assets/shaders/Renderer2D_Circle.glsl"),
            BufferLayout{
                { "a_WorldPosition", ShaderDataType::Float3 },
                { "a_LocalPosition", ShaderDataType::Float3 },
                { "a_Color",         ShaderDataType::Float4 },
                { "a_Thickness",     ShaderDataType::Float  },
                { "a_Fade",          ShaderDataType::Float  },
                { "a_EntityID",      ShaderDataType::Int    }
            },
            BatchDrawMode::Triangles,
            sharedIBO
        );

        s_Data.LineBatch = CreateScope<RenderBatch<LineVertex>>(
            k_MaxVertices,
            Shader::Create("assets/shaders/Renderer2D_Line.glsl"),
            BufferLayout{
                { "a_Position", ShaderDataType::Float3 },
                { "a_Color",    ShaderDataType::Float4 },
                { "a_EntityID", ShaderDataType::Int    }
            },
            BatchDrawMode::Lines
        );

        s_Data.TextBatch = CreateScope<RenderBatch<TextVertex>>(
            k_MaxVertices,
            Shader::Create("assets/shaders/Renderer2D_TextSDF.glsl"),
            BufferLayout{
                { "a_Position",     ShaderDataType::Float3 },
                { "a_Color",        ShaderDataType::Float4 },
                { "a_TexCoord",     ShaderDataType::Float2 },
                { "a_TexIndex",     ShaderDataType::Float  },
                { "a_OutlineColor", ShaderDataType::Float4 },
                { "a_OutlineWidth", ShaderDataType::Float  },
                { "a_EdgeSoftness", ShaderDataType::Float  },
                { "a_EntityID",     ShaderDataType::Int    }
            },
            BatchDrawMode::Triangles,
            sharedIBO
        );

        s_Data.TextBatch->GetShader()->Bind();
        s_Data.TextBatch->GetShader()->SetIntArray("u_Textures", samplers, k_MaxTexSlots);
        s_Data.TextBatch->SetPreDrawCallback([]()
            {
                for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++)
                    s_Data.TextureSlots[i]->Bind(i);
            });

        s_Data.CameraUniformBuffer = UniformBuffer::Create(
            sizeof(Renderer2DData::CameraData), 0);
    }

    void Renderer2D::Shutdown()
    {
        WT_PROFILE_FUNCTION();
        s_Data.QuadBatch.reset();
        s_Data.CircleBatch.reset();
        s_Data.LineBatch.reset();
        s_Data.TextBatch.reset();
    }


    static void UploadCameraAndReset(const glm::mat4& vp)
    {
        s_Data.CameraBuffer.ViewProjection = vp;
        s_Data.CameraUniformBuffer->SetData(
            &s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));

        s_Data.QuadBatch->Reset();
        s_Data.CircleBatch->Reset();
        s_Data.LineBatch->Reset();
        s_Data.TextBatch->Reset();
        s_Data.TextureSlotIndex = 1;
    }

    void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
    {
        WT_PROFILE_FUNCTION();
        UploadCameraAndReset(camera.GetProjection() * glm::inverse(transform));
    }

    void Renderer2D::BeginScene(const EditorCamera& camera)
    {
        WT_PROFILE_FUNCTION();
        UploadCameraAndReset(camera.GetViewProjection());
    }

    void Renderer2D::EndScene()
    {
        WT_PROFILE_FUNCTION();
        Flush();
    }

    void Renderer2D::SetViewProjection(const glm::mat4& vp)
    {
        s_Data.CameraBuffer.ViewProjection = vp;
        s_Data.CameraUniformBuffer->SetData(
            &s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
    }

    //  Flush

    void Renderer2D::Flush()
    {
        WT_PROFILE_FUNCTION();
        s_Data.QuadBatch->Flush(s_Data.Stats);
        s_Data.TextBatch->Flush(s_Data.Stats);
        s_Data.CircleBatch->Flush(s_Data.Stats);
        s_Data.LineBatch->Flush(s_Data.Stats);
        s_Data.QuadBatch->Reset();
        s_Data.CircleBatch->Reset();
        s_Data.LineBatch->Reset();
        s_Data.TextBatch->Reset();
        s_Data.TextureSlotIndex = 1;
    }

    static void FlushAndReset()
    {
        Renderer2D::Flush();
        s_Data.QuadBatch->Reset();
        s_Data.CircleBatch->Reset();
        s_Data.LineBatch->Reset();
        s_Data.TextBatch->Reset();
        s_Data.TextureSlotIndex = 1;
    }


    static float GetOrAllocTextureSlot(const Ref<Texture2D>& texture)
    {
        for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
            if (*s_Data.TextureSlots[i] == *texture)
                return static_cast<float>(i);

        if (s_Data.TextureSlotIndex >= k_MaxTexSlots)
            FlushAndReset();

        const float index = static_cast<float>(s_Data.TextureSlotIndex);
        WT_CORE_ASSERT(s_Data.TextureSlotIndex < k_MaxTexSlots, "Texture slot overflow");
        s_Data.TextureSlots[s_Data.TextureSlotIndex++] = texture;
        return index;
    }

    static void SubmitQuadVertices(
        const glm::mat4& transform,
        const glm::vec4& color,
        const glm::vec2  uvs[4],
        float texIndex,
        float tilingFactor,
        int   entityID)
    {
        if (s_Data.QuadBatch->IsFull(4))
            FlushAndReset();

        for (int i = 0; i < 4; i++)
        {
            auto* v = s_Data.QuadBatch->AllocVertex();
            v->Position = transform * k_QuadPositions[i];
            v->Color = color;
            v->TexCoord = uvs[i];
            v->TexIndex = texIndex;
            v->TilingFactor = tilingFactor;
            v->EntityID = entityID;
        }
        s_Data.QuadBatch->AddIndexCount(6);
        s_Data.Stats.QuadCount++;
    }

    //  DrawQuad

    void Renderer2D::DrawQuad(const glm::mat4& transform,
        const glm::vec4& color, int entityID)
    {
        WT_PROFILE_FUNCTION();
        SubmitQuadVertices(transform, color, k_DefaultUV, 0.0f, 1.0f, entityID);
    }

    void Renderer2D::DrawQuad(const glm::mat4& transform,
        const Ref<Texture2D>& texture,
        float tilingFactor, const glm::vec4& tintColor,
        int entityID)
    {
        WT_PROFILE_FUNCTION();
        const float texIndex = GetOrAllocTextureSlot(texture);
        SubmitQuadVertices(transform, tintColor, k_DefaultUV,
            texIndex, tilingFactor, entityID);
    }

    void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size,
        const glm::vec4& color)
    {
        DrawQuad(glm::vec3(pos, 0.0f), size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size,
        const glm::vec4& color)
    {
        DrawQuad(
            glm::scale(glm::translate(glm::mat4(1.0f), pos),
                { size.x, size.y, 1.0f }),
            color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size,
        const Ref<Texture2D>& texture,
        float tilingFactor, const glm::vec4& tintColor)
    {
        DrawQuad(
            glm::scale(glm::translate(glm::mat4(1.0f), pos),
                { size.x, size.y, 1.0f }),
            texture, tilingFactor, tintColor);
    }

    void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size,
        const Ref<SubTexture2D>& subTexture,
        float tilingFactor, const glm::vec4& tintColor)
    {
        const glm::mat4 transform =
            glm::scale(glm::translate(glm::mat4(1.0f), pos),
                { size.x, size.y, 1.0f });
        const float texIndex = GetOrAllocTextureSlot(subTexture->GetTexture());
        SubmitQuadVertices(transform, tintColor,
            subTexture->GetTexCoords(), texIndex, tilingFactor, -1);
    }

    //  DrawCircle

    void Renderer2D::DrawCircle(const glm::mat4& transform,
        const glm::vec4& color,
        float thickness, float fade, int entityID)
    {
        WT_PROFILE_FUNCTION();

        if (s_Data.CircleBatch->IsFull(4))
            FlushAndReset();

        for (int i = 0; i < 4; i++)
        {
            auto* v = s_Data.CircleBatch->AllocVertex();
            v->WorldPosition = transform * k_QuadPositions[i];
            v->LocalPosition = k_QuadPositions[i] * 2.0f;
            v->Color = color;
            v->Thickness = thickness;
            v->Fade = fade;
            v->EntityID = entityID;
        }
        s_Data.CircleBatch->AddIndexCount(6);
        s_Data.Stats.QuadCount++;
    }

    //  DrawSprite / DrawAnimationFrame

    void Renderer2D::DrawSprite(const glm::mat4& transform,
        SpriteRendererComponent& src, int entityID)
    {
        glm::mat4 drawTransform = transform;
        const bool hasDrawOffset = src.DrawOffset.x != 0.0f || src.DrawOffset.y != 0.0f;
        const bool hasDrawScale = src.DrawScale.x != 1.0f || src.DrawScale.y != 1.0f;
        if (hasDrawOffset || hasDrawScale)
        {
            drawTransform *= glm::translate(glm::mat4(1.0f), { src.DrawOffset.x, src.DrawOffset.y, 0.0f });
            drawTransform *= glm::scale(glm::mat4(1.0f), { src.DrawScale.x, src.DrawScale.y, 1.0f });
        }

        if (src.Texture)
            DrawAnimationFrame(drawTransform, src.Texture,
                src.UVMin, src.UVMax, src.FlipX,
                src.Color, entityID);
        else
            DrawQuad(drawTransform, src.Color, entityID);
    }

    void Renderer2D::DrawAnimationFrame(const glm::mat4& transform,
        const Ref<Texture2D>& texture,
        const glm::vec2& uvMin,
        const glm::vec2& uvMax,
        bool flipX,
        const glm::vec4& tintColor,
        int entityID)
    {
        WT_PROFILE_FUNCTION();

        float uL = flipX ? uvMax.x : uvMin.x;
        float uR = flipX ? uvMin.x : uvMax.x;

        const glm::vec2 uvs[4] = {
            { uL, uvMin.y }, { uR, uvMin.y },
            { uR, uvMax.y }, { uL, uvMax.y },
        };

        const float texIndex = GetOrAllocTextureSlot(texture);
        SubmitQuadVertices(transform, tintColor, uvs, texIndex, 1.0f, entityID);
    }

    void Renderer2D::DrawTextGlyph(const glm::mat4& transform,
        const Ref<Texture2D>& texture,
        const glm::vec2& uvMin,
        const glm::vec2& uvMax,
        const glm::vec4& fillColor,
        const glm::vec4& outlineColor,
        float outlineWidth,
        float edgeSoftness,
        int entityID)
    {
        WT_PROFILE_FUNCTION();

        if (s_Data.TextBatch->IsFull(4))
            FlushAndReset();

        const float texIndex = GetOrAllocTextureSlot(texture);
        const glm::vec2 uvs[4] = {
            { uvMin.x, uvMin.y },
            { uvMax.x, uvMin.y },
            { uvMax.x, uvMax.y },
            { uvMin.x, uvMax.y },
        };

        for (int i = 0; i < 4; i++)
        {
            auto* v = s_Data.TextBatch->AllocVertex();
            v->Position = transform * k_QuadPositions[i];
            v->Color = fillColor;
            v->TexCoord = uvs[i];
            v->TexIndex = texIndex;
            v->OutlineColor = outlineColor;
            v->OutlineWidth = outlineWidth;
            v->EdgeSoftness = edgeSoftness;
            v->EntityID = entityID;
        }
        s_Data.TextBatch->AddIndexCount(6);
        s_Data.Stats.QuadCount++;
    }


    void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec4& color, int entityID)
    {
        if (s_Data.LineBatch->IsFull(2))
            FlushAndReset();

        auto* v0 = s_Data.LineBatch->AllocVertex();
        v0->Position = p0; v0->Color = color; v0->EntityID = entityID;

        auto* v1 = s_Data.LineBatch->AllocVertex();
        v1->Position = p1; v1->Color = color; v1->EntityID = entityID;

        s_Data.LineBatch->AddVertexCount(2);
    }

    void Renderer2D::DrawPolyline(const std::vector<glm::vec3>& points,
        const glm::vec4& color,
        int entityID)
    {
        if (points.size() < 2)
            return;

        for (size_t i = 1; i < points.size(); ++i)
            DrawLine(points[i - 1], points[i], color, entityID);
    }

    void Renderer2D::DrawRect(const glm::vec3& pos, const glm::vec2& size,
        const glm::vec4& color, int entityID)
    {
        const float hx = size.x * 0.5f, hy = size.y * 0.5f, z = pos.z;
        DrawLine({ pos.x - hx, pos.y - hy, z }, { pos.x + hx, pos.y - hy, z }, color, entityID);
        DrawLine({ pos.x + hx, pos.y - hy, z }, { pos.x + hx, pos.y + hy, z }, color, entityID);
        DrawLine({ pos.x + hx, pos.y + hy, z }, { pos.x - hx, pos.y + hy, z }, color, entityID);
        DrawLine({ pos.x - hx, pos.y + hy, z }, { pos.x - hx, pos.y - hy, z }, color, entityID);
    }

    void Renderer2D::DrawRect(const glm::mat4& transform,
        const glm::vec4& color, int entityID)
    {
        glm::vec3 corners[4];
        for (int i = 0; i < 4; i++)
            corners[i] = transform * k_QuadPositions[i];

        DrawLine(corners[0], corners[1], color, entityID);
        DrawLine(corners[1], corners[2], color, entityID);
        DrawLine(corners[2], corners[3], color, entityID);
        DrawLine(corners[3], corners[0], color, entityID);
    }


    float Renderer2D::GetLineWidth() { return s_Data.LineBatch->GetLineWidth(); }
    void  Renderer2D::SetLineWidth(float w) { s_Data.LineBatch->SetLineWidth(w); }

    void Renderer2D::ResetStats()
    {
        std::memset(&s_Data.Stats, 0, sizeof(Statistics));
    }

    Renderer2D::Statistics Renderer2D::GetStats()
    {
        return s_Data.Stats;
    }

} // namespace Wheatear
