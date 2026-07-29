#pragma once

#include "Wheatear/Core/Core.h"
#include "Camera.h"
#include "EditorCamera.h"
#include "Texture.h"
#include "SubTexture2D.h"

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace Wheatear {

    struct SpriteRendererComponent;

    /// 2D 鎵瑰鐞嗘覆鏌撳櫒
    /// 
    /// 瀵瑰鎺ュ彛涓庡師鐗堝畬鍏ㄤ竴鑷达紝鍐呴儴鏀逛负鍩轰簬 RenderBatch<T> 鐨勫彲鎵╁睍鏋舵瀯銆?
    /// 
    /// 鏂板鍥惧厓姝ラ锛?
    ///   1. 鍦?Renderer2DVertices.h 閲屽姞椤剁偣缁撴瀯浣?
    ///   2. 鍦?Renderer2DData锛堝唴閮級閲屽姞 RenderBatch<鏂癡ertex> 瀹炰緥
    ///   3. 鍦?Init() 閲屽垵濮嬪寲 batch
    ///   4. 鍦?Flush() 閲?batch.Flush()锛堝凡鏄惊鐜紝鑷姩澶勭悊锛?
    ///   5. 鍐?DrawXxx 鍏叡鍑芥暟
    ///   浠ヤ笂鏀瑰姩鍏ㄩ儴灞€闄愬湪 Renderer2D.cpp锛?h 鍙渶鍔?DrawXxx 澹版槑
    class Renderer2D
    {
    public:
        static void Init();
        static void Shutdown();

        // 鈹€鈹€ 鍦烘櫙寮€濮?缁撴潫 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void BeginScene(const EditorCamera& camera);
        static void EndScene();
        static void Flush();

        static void SetViewProjection(const glm::mat4& viewProjection);

        // 鈹€鈹€ Quad 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        static void DrawQuad(const glm::mat4& transform,
            const glm::vec4& color, int entityID = -1);
        static void DrawQuad(const glm::mat4& transform,
            const Ref<Texture2D>& texture,
            float tilingFactor = 1.0f,
            const glm::vec4& tintColor = glm::vec4(1.0f),
            int entityID = -1);

        // 渚挎嵎閲嶈浇锛堝唴閮ㄨ浆涓?mat4锛?
        static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
            const glm::vec4& color);
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
            const glm::vec4& color);
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
            const Ref<Texture2D>& texture,
            float tilingFactor = 1.0f,
            const glm::vec4& tintColor = glm::vec4(1.0f));
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
            const Ref<SubTexture2D>& subTexture,
            float tilingFactor = 1.0f,
            const glm::vec4& tintColor = glm::vec4(1.0f));

        // 鈹€鈹€ Circle 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        static void DrawCircle(const glm::mat4& transform,
            const glm::vec4& color,
            float thickness = 1.0f,
            float fade = 0.005f,
            int entityID = -1);

        // 鈹€鈹€ Sprite锛堢敱鍦烘櫙绯荤粺璋冪敤锛夆攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        static void DrawSprite(const glm::mat4& transform,
            SpriteRendererComponent& src,
            int entityID);

        // 鈹€鈹€ Animation Frame锛堣嚜瀹氫箟 UV锛夆攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        static void DrawAnimationFrame(const glm::mat4& transform,
            const Ref<Texture2D>& texture,
            const glm::vec2& uvMin,
            const glm::vec2& uvMax,
            bool flipX,
            const glm::vec4& tintColor = glm::vec4(1.0f),
            int entityID = -1);

        static void DrawTextGlyph(const glm::mat4& transform,
            const Ref<Texture2D>& texture,
            const glm::vec2& uvMin,
            const glm::vec2& uvMax,
            const glm::vec4& fillColor,
            const glm::vec4& outlineColor,
            float outlineWidth,
            float edgeSoftness,
            int entityID = -1);

        // 鈹€鈹€ Line / Rect 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        static void DrawLine(const glm::vec3& p0, const glm::vec3& p1,
            const glm::vec4& color, int entityID = -1);
        static void DrawPolyline(const std::vector<glm::vec3>& points,
            const glm::vec4& color,
            int entityID = -1);
        static void DrawRect(const glm::vec3& position, const glm::vec2& size,
            const glm::vec4& color, int entityID = -1);
        static void DrawRect(const glm::mat4& transform,
            const glm::vec4& color, int entityID = -1);

        static float GetLineWidth();
        static void  SetLineWidth(float width);

        // 鈹€鈹€ 缁熻 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        struct Statistics
        {
            uint32_t DrawCalls = 0;
            uint32_t QuadCount = 0;

            uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
            uint32_t GetTotalIndexCount()  const { return QuadCount * 6; }
        };

        static void       ResetStats();
        static Statistics GetStats();

    private:
        
    };

} // namespace Wheatear
