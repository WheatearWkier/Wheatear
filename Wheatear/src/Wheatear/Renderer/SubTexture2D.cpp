#include "wtpch.h"
#include "SubTexture2D.h"

namespace Wheatear {

    SubTexture2D::SubTexture2D(const Ref<Texture2D>& texture,
        const glm::vec2& min,
        const glm::vec2& max)
        : m_Texture(texture)
    {
        // 逆时针排列：左下 → 右下 → 右上 → 左上
        m_TexCoords[0] = { min.x, min.y };
        m_TexCoords[1] = { max.x, min.y };
        m_TexCoords[2] = { max.x, max.y };
        m_TexCoords[3] = { min.x, max.y };
    }

    Ref<SubTexture2D> SubTexture2D::CreateFromCoords(
        const Ref<Texture2D>& texture,
        const glm::vec2& coords,
        const glm::vec2& cellSize,
        const glm::vec2& spriteSize)
    {
        // 将格子索引转换为归一化 UV 坐标
        const float texW = static_cast<float>(texture->GetWidth());
        const float texH = static_cast<float>(texture->GetHeight());

        const glm::vec2 min = {
            (coords.x * cellSize.x) / texW,
            (coords.y * cellSize.y) / texH
        };
        const glm::vec2 max = {
            ((coords.x + spriteSize.x) * cellSize.x) / texW,
            ((coords.y + spriteSize.y) * cellSize.y) / texH
        };

        return CreateRef<SubTexture2D>(texture, min, max);
    }

} // namespace Wheatear