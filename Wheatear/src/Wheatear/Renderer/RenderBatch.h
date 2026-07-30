#pragma once

#include "VertexArray.h"
#include "Shader.h"
#include "RenderCommand.h"
#include "Wheatear/Renderer/Renderer2D.h"

#include <vector>
#include <functional>

namespace Wheatear {

    enum class BatchDrawMode
    {
        Triangles,
        Lines
    };

    /// 
    ///   - VAO / VBO / IBO
    ///   - Flush / Reset
    /// 
    /// 
    template<typename TVertex>
    class RenderBatch
    {
    public:
        RenderBatch(uint32_t maxVertices,
            Ref<Shader> shader,
            const BufferLayout& layout,
            BatchDrawMode drawMode,
            Ref<IndexBuffer> indexBuffer = nullptr)
            : m_Shader(std::move(shader))
            , m_DrawMode(drawMode)
            , m_MaxVertices(maxVertices)
        {
            m_VertexBufferBase = new TVertex[maxVertices];
            m_VertexBufferPtr = m_VertexBufferBase;

            // VAO + VBO
            m_VertexArray = VertexArray::Create();
            m_VertexBuffer = VertexBuffer::Create(maxVertices * sizeof(TVertex));
            m_VertexBuffer->SetLayout(layout);
            m_VertexArray->AddVertexBuffer(m_VertexBuffer);

            if (indexBuffer)
                m_VertexArray->SetIndexBuffer(indexBuffer);
        }

        ~RenderBatch()
        {
            delete[] m_VertexBufferBase;
        }

        RenderBatch(const RenderBatch&) = delete;
        RenderBatch& operator=(const RenderBatch&) = delete;
        RenderBatch(RenderBatch&&) = default;

        TVertex* AllocVertex()
        {
            return m_VertexBufferPtr++;
        }

        void AddIndexCount(uint32_t count) { m_IndexCount += count; }

        void AddVertexCount(uint32_t count) { m_VertexCount += count; }

        bool IsEmpty() const
        {
            return m_DrawMode == BatchDrawMode::Lines
                ? m_VertexCount == 0
                : m_IndexCount == 0;
        }

        bool IsFull(uint32_t extraVertices = 4) const
        {
            return (m_VertexBufferPtr - m_VertexBufferBase) + extraVertices > m_MaxVertices;
        }

        uint32_t Flush(Renderer2D::Statistics& stats)
        {
            if (IsEmpty()) return 0;

            const size_t dataSize =
                (m_VertexBufferPtr - m_VertexBufferBase) * sizeof(TVertex);
            m_VertexBuffer->SetData(m_VertexBufferBase, (uint32_t)dataSize);

            OnPreDraw();

            m_Shader->Bind();

            if (m_DrawMode == BatchDrawMode::Lines)
            {
                RenderCommand::SetLineWidth(m_LineWidth);
                RenderCommand::DrawLines(m_VertexArray, m_VertexCount);
            }
            else
            {
                RenderCommand::DrawIndexed(m_VertexArray, m_IndexCount);
            }

            stats.DrawCalls++;
            return 1;
        }

        void Reset()
        {
            m_VertexBufferPtr = m_VertexBufferBase;
            m_IndexCount = 0;
            m_VertexCount = 0;
        }

        void SetLineWidth(float w) { m_LineWidth = w; }
        float GetLineWidth()  const { return m_LineWidth; }

        Ref<Shader>& GetShader() { return m_Shader; }

        void SetPreDrawCallback(std::function<void()> cb) { m_PreDrawCallback = std::move(cb); }

    private:
        void OnPreDraw()
        {
            if (m_PreDrawCallback) m_PreDrawCallback();
        }

    private:
        Ref<VertexArray>  m_VertexArray;
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<Shader>       m_Shader;
        BatchDrawMode     m_DrawMode;

        uint32_t  m_MaxVertices = 0;
        uint32_t  m_IndexCount = 0;
        uint32_t  m_VertexCount = 0;
        float     m_LineWidth = 2.0f;

        TVertex* m_VertexBufferBase = nullptr;
        TVertex* m_VertexBufferPtr = nullptr;

        std::function<void()> m_PreDrawCallback;
    };

} // namespace Wheatear
