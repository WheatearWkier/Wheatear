#include "wtpch.h"
#include "SideCombatFeedbackService.h"

#include "Wheatear/Core/UserSettings.h"
#include "Wheatear/Modules/Common/GameplayAudioService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>

namespace Wheatear::SideCombatFeedbackService {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetImageAlpha;

        static Entity FindFeedbackCamera(Scene* scene)
        {
            if (!scene)
                return {};

            Entity camera = scene->GetPrimaryCameraEntity();
            if (!camera || !camera.HasComponent<TransformComponent>())
                camera = FindEntityByName(scene, "SC_Camera");
            return camera;
        }

    } // namespace

    void PlaySfx(const std::string& path, float volume)
    {
        if (path.empty())
            return;

        GameplayAudioService::PlaySFX(path, volume);
    }

    void TriggerHitFeedback(Scene* scene,
        SideCombatLevelComponent& level,
        const SideHitboxComponent& hitbox)
    {
        PlaySfx(hitbox.HitSound, hitbox.HitSoundVolume);
        level.RuntimeHitPauseTimer = std::max(level.RuntimeHitPauseTimer, std::max(0.0f, hitbox.HitPause));

        if (!scene || !UserSettings::Get().ScreenShake || hitbox.CameraShake <= 0.0f)
            return;

        Entity camera = FindFeedbackCamera(scene);
        if (!camera || !camera.HasComponent<TransformComponent>())
            return;

        auto& transform = camera.GetComponent<TransformComponent>();
        if (!level.RuntimeCameraBaseCaptured)
        {
            level.RuntimeCameraBaseTranslation = transform.Translation;
            level.RuntimeCameraBaseCaptured = true;
        }

        level.RuntimeCameraShakeTimer = std::max(level.RuntimeCameraShakeTimer, std::max(0.01f, hitbox.CameraShakeDuration));
        level.RuntimeCameraShakeDuration = std::max(level.RuntimeCameraShakeDuration, std::max(0.01f, hitbox.CameraShakeDuration));
        level.RuntimeCameraShakeStrength = std::max(level.RuntimeCameraShakeStrength, hitbox.CameraShake);
    }

    void UpdateCameraFeedback(Scene* scene,
        SideCombatLevelComponent& level,
        float dt)
    {
        if (!scene || !level.RuntimeCameraBaseCaptured)
            return;

        Entity camera = FindFeedbackCamera(scene);
        if (!camera || !camera.HasComponent<TransformComponent>())
            return;

        auto& transform = camera.GetComponent<TransformComponent>();
        if (level.RuntimeCameraShakeTimer <= 0.0f || !UserSettings::Get().ScreenShake)
        {
            transform.Translation = level.RuntimeCameraBaseTranslation;
            level.RuntimeCameraShakeTimer = 0.0f;
            level.RuntimeCameraShakeDuration = 0.0f;
            level.RuntimeCameraShakeStrength = 0.0f;
            level.RuntimeCameraBaseCaptured = false;
            return;
        }

        level.RuntimeCameraShakeTimer = std::max(0.0f, level.RuntimeCameraShakeTimer - dt);
        const float duration = std::max(0.01f, level.RuntimeCameraShakeDuration);
        const float normalized = level.RuntimeCameraShakeTimer / duration;
        const float strength = level.RuntimeCameraShakeStrength * normalized * normalized;
        const float phase = level.RuntimeElapsed * 95.0f;
        transform.Translation = level.RuntimeCameraBaseTranslation + glm::vec3(
            std::sin(phase) * strength,
            std::sin(phase * 1.37f + 1.6f) * strength * 0.62f,
            0.0f);
    }

    void UpdateStartFade(Scene* scene,
        SideCombatLevelComponent& level,
        float dt)
    {
        if (level.StartFadeDuration <= 0.0f)
        {
            level.RuntimeFadeAlpha = 0.0f;
            SetImageAlpha(scene, level.FadeEntityName, 0.0f);
            return;
        }

        level.RuntimeFadeAlpha = std::max(0.0f,
            level.RuntimeFadeAlpha - dt / level.StartFadeDuration);
        SetImageAlpha(scene, level.FadeEntityName, level.RuntimeFadeAlpha);
    }

} // namespace Wheatear::SideCombatFeedbackService
