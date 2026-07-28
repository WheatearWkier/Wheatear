#pragma once

#include "Wheatear/Core/Core.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Scene/Scene.h"
#include "SceneHierarchy/Drawers/SpriteAnimatorDrawer.h"  // AtlasConfig

#include <imgui/imgui.h>

#include <string>
#include <unordered_map>

namespace Wheatear {

    class AnimationEditorPanel
    {
    public:
        void OnImGuiRender(Timestep ts);

        // 从 SceneHierarchyPanel 选中实体时调用
        void SetEntity(Entity entity);

        // 获取当前 Scene 以播放AnimationClip
        void SetScene(const Ref<Scene>& scene) { m_Scene = scene; }

    private:
        void DrawToolbar();
        void DrawTimeline();
        void DrawAtlasSection();
        void DrawFramesSection();

        // 时间轴内部
        void DrawFrameTrack(ImVec2 origin, float trackHeight);
        void DrawPropertyTrack(ImVec2 origin, float trackHeight, int trackIndex);

        Ref<AnimationClip> GetCurrentClip() const;

        // 同步动画帧至SpriteRendererComponent
        void SyncPreviewToEntity();

        // 快照
        void TakeSnapshot();
        void RestoreSnapshot();
        void StopPreview();

        void BeginScrub();
        void EndScrub();

    private:
        // ── 绑定的实体和组件 ─────────────────────────────────
        Entity                      m_Entity;
        SpriteAnimatorComponent* m_Animator = nullptr;  // 指向组件，选中时绑定

        std::string m_DefaultClipName;  // 设计时设定，序列化，不被运行时修改
        std::string m_CurrentClipName;  // 运行时当前播放的，会被脚本修改

        // ── 编辑器预览播放状态 ───────────────────────────────
        Ref<Scene> m_Scene;
        float   m_PlaybackTime = 0.0f;
        bool    m_IsPlaying = false;

        // ── 时间轴显示参数 ───────────────────────────────────
        float   m_PixelsPerSecond = 120.0f;   // 缩放：像素/秒
        float   m_ScrollX = 0.0f;     // 水平滚动偏移（像素）

        // 拖动时间指针
        bool    m_DraggingCursor = false;

        // ── Atlas 配置（从 SceneHierarchyPanel 搬过来）───────
        std::unordered_map<std::string, AtlasConfig> m_AtlasConfigs;

        // 预览前的原始组件状态快照
        glm::vec4 m_SnapshotColor = glm::vec4(1.0f);
        glm::vec3 m_SnapshotTranslation;
        glm::vec3 m_SnapshotRotation;
        glm::vec3 m_SnapshotScale = glm::vec3(1.0f);

        Ref<Texture2D> m_SnapshotTexture;
        glm::vec2      m_SnapshotTexCoordMin = { 0.0f, 0.0f };
        glm::vec2      m_SnapshotTexCoordMax = { 1.0f, 1.0f };
        bool           m_IsScrubbing = false;
        bool           m_HasSnapshot = false;
    };

} // namespace Wheatear