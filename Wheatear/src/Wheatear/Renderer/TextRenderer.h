#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Renderer/Font.h"

#include <glm/glm.hpp>

#include <string>

#ifdef DrawText
#undef DrawText
#endif

namespace Wheatear {

    struct TextRenderParams
    {
        float Scale = 1.0f;
        float ScaleX = 0.0f;
        float ScaleY = 0.0f;
        float LineSpacing = 1.0f;
        float LetterSpacing = 0.0f;
        float WrapWidth = 0.0f;
        float MaxHeight = 0.0f;
        glm::vec4 ClipRect = { -1.0f, -1.0f, 1.0f, 1.0f };
        bool Clip = false;
        glm::vec4 OutlineColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        float OutlineWidth = 0.0f;
        float EdgeSoftness = 0.0f;
        int   EntityID = -1;
    };

    class WHEATEAR_API TextRenderer
    {
    public:
        static void DrawText(const Ref<Font>& font,
            const std::string& text,
            const glm::vec3& topLeft,
            const glm::vec4& color,
            const TextRenderParams& params = {});

        static glm::vec2 MeasureText(const Ref<Font>& font,
            const std::string& text,
            const TextRenderParams& params = {});

        static Ref<Font> GetDefaultFont();
        static void SetDefaultFont(const Ref<Font>& font);

    private:
        static Ref<Font> s_DefaultFont;
    };

} // namespace Wheatear
