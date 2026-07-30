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

    struct SpriteAnimatorComponent;

    class AnimationEditorPanel
    {
    public:
        void OnImGuiRender(Timestep ts);

        void SetEntity(Entity entity);

        void SetScene(const Ref<Scene>& scene) { m_Scene = scene; }

    private:
        void DrawToolbar();
        void DrawTimeline();
        void DrawAtlasSection();
        void DrawFramesSection();
        void DrawEventsSection();

        void DrawEventTrack(ImVec2 origin, float trackHeight);
        void DrawFrameTrack(ImVec2 origin, float trackHeight);
        void DrawPropertyTrack(ImVec2 origin, float trackHeight, int trackIndex);

        Ref<AnimationClip> GetCurrentClip() const;

        void SyncPreviewToEntity();

        void TakeSnapshot();
        void RestoreSnapshot();
        void StopPreview();

        void BeginScrub();
        void EndScrub();

    private:
        Entity                      m_Entity;
        SpriteAnimatorComponent* m_Animator = nullptr;

        std::string m_DefaultClipName;
        std::string m_CurrentClipName;

        Ref<Scene> m_Scene;
        float   m_PlaybackTime = 0.0f;
        bool    m_IsPlaying = false;

        float   m_PixelsPerSecond = 120.0f;
        float   m_ScrollX = 0.0f;

        bool    m_DraggingCursor = false;

        std::unordered_map<std::string, AtlasConfig> m_AtlasConfigs;

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
