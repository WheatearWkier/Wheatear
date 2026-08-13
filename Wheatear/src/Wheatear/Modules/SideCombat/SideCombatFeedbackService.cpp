#include "wtpch.h"
#include "SideCombatFeedbackService.h"

#include "Wheatear/Core/UserSettings.h"
#include "Wheatear/Gameplay/Services/GameplayAudioService.h"
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

        static float SmoothStep01(float value)
        {
            value = std::clamp(value, 0.0f, 1.0f);
            return value * value * (3.0f - 2.0f * value);
        }

        static Entity FindFeedbackCamera(Scene* scene, const SideCombatLevelComponent& level)
        {
            if (!scene)
                return {};

            Entity camera = scene->GetPrimaryCameraEntity();
            if (!camera || !camera.HasComponent<TransformComponent>())
                camera = FindEntityByName(scene, level.CameraEntityName);
            return camera;
        }

        static void CaptureCameraBase(Entity camera,
            SideCombatLevelComponent& level)
        {
            if (!camera || !camera.HasComponent<TransformComponent>())
                return;

            auto& transform = camera.GetComponent<TransformComponent>();
            if (!level.RuntimeCameraBaseCaptured)
            {
                level.RuntimeCameraBaseTranslation = transform.Translation;
                level.RuntimeCameraBaseCaptured = true;
            }

            if (!camera.HasComponent<CameraComponent>())
                return;

            auto& cameraComponent = camera.GetComponent<CameraComponent>();
            if (cameraComponent.Camera.GetProjectionType() != SceneCamera::ProjectionType::Orthographic ||
                level.RuntimeCameraProjectionCaptured)
            {
                return;
            }

            level.RuntimeCameraBaseOrthographicSize = cameraComponent.Camera.GetOrthographicSize();
            level.RuntimeCameraProjectionCaptured = true;
        }

        static void RestoreCameraBase(Entity camera,
            SideCombatLevelComponent& level)
        {
            if (!camera)
                return;

            if (camera.HasComponent<TransformComponent>() && level.RuntimeCameraBaseCaptured)
                camera.GetComponent<TransformComponent>().Translation = level.RuntimeCameraBaseTranslation;

            if (camera.HasComponent<CameraComponent>() &&
                level.RuntimeCameraProjectionCaptured)
            {
                auto& cameraComponent = camera.GetComponent<CameraComponent>();
                if (cameraComponent.Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
                {
                    cameraComponent.Camera.SetOrthographicSize(
                        std::max(0.01f, level.RuntimeCameraBaseOrthographicSize));
                }
            }
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

        Entity camera = FindFeedbackCamera(scene, level);
        if (!camera || !camera.HasComponent<TransformComponent>())
            return;

        CaptureCameraBase(camera, level);

        level.RuntimeCameraShakeTimer = std::max(level.RuntimeCameraShakeTimer, std::max(0.01f, hitbox.CameraShakeDuration));
        level.RuntimeCameraShakeDuration = std::max(level.RuntimeCameraShakeDuration, std::max(0.01f, hitbox.CameraShakeDuration));
        level.RuntimeCameraShakeStrength = std::max(level.RuntimeCameraShakeStrength, hitbox.CameraShake);
    }

    void TriggerCinematicFocus(Scene* scene,
        SideCombatLevelComponent& level,
        UUID focusEntity,
        float duration,
        float timeScale,
        float cameraZoom,
        const glm::vec2& cameraOffset)
    {
        if (!scene || static_cast<uint64_t>(focusEntity) == 0 ||
            duration <= 0.0f)
        {
            return;
        }

        Entity camera = FindFeedbackCamera(scene, level);
        if (!camera || !camera.HasComponent<TransformComponent>())
            return;

        CaptureCameraBase(camera, level);
        level.RuntimeCinematicTimer = std::max(level.RuntimeCinematicTimer, duration);
        level.RuntimeCinematicDuration = std::max(level.RuntimeCinematicDuration, duration);
        level.RuntimeCinematicTimeScale = std::clamp(timeScale, 0.02f, 1.0f);
        level.RuntimeCinematicCameraZoom = std::max(1.0f, cameraZoom);
        level.RuntimeCinematicCameraOffset = cameraOffset;
        level.RuntimeCinematicFocusEntity = focusEntity;
    }

    void UpdateCameraFeedback(Scene* scene,
        SideCombatLevelComponent& level,
        float dt)
    {
        if (!scene)
            return;

        Entity camera = FindFeedbackCamera(scene, level);
        if (!camera || !camera.HasComponent<TransformComponent>())
            return;

        const bool hadFocus = level.RuntimeCinematicTimer > 0.0f &&
            static_cast<uint64_t>(level.RuntimeCinematicFocusEntity) != 0;
        const bool hadShake = level.RuntimeCameraShakeTimer > 0.0f &&
            UserSettings::Get().ScreenShake;
        if (!hadFocus && !hadShake && !level.RuntimeCameraBaseCaptured &&
            !level.RuntimeCameraProjectionCaptured)
        {
            return;
        }

        if (hadFocus || hadShake)
            CaptureCameraBase(camera, level);

        level.RuntimeCinematicTimer = std::max(
            0.0f,
            level.RuntimeCinematicTimer - dt);
        level.RuntimeCameraShakeTimer = std::max(0.0f, level.RuntimeCameraShakeTimer - dt);

        const bool focusActive = level.RuntimeCinematicTimer > 0.0f &&
            static_cast<uint64_t>(level.RuntimeCinematicFocusEntity) != 0;
        const bool shakeActive = level.RuntimeCameraShakeTimer > 0.0f &&
            UserSettings::Get().ScreenShake;
        if (!focusActive && !shakeActive)
        {
            RestoreCameraBase(camera, level);
            level.RuntimeCinematicTimer = 0.0f;
            level.RuntimeCinematicDuration = 0.0f;
            level.RuntimeCinematicTimeScale = 1.0f;
            level.RuntimeCinematicCameraZoom = 1.0f;
            level.RuntimeCinematicCameraOffset = { 0.0f, 0.0f };
            level.RuntimeCinematicFocusEntity = 0;
            level.RuntimeCameraShakeTimer = 0.0f;
            level.RuntimeCameraShakeDuration = 0.0f;
            level.RuntimeCameraShakeStrength = 0.0f;
            level.RuntimeCameraBaseCaptured = false;
            level.RuntimeCameraProjectionCaptured = false;
            return;
        }

        auto& transform = camera.GetComponent<TransformComponent>();
        glm::vec3 translation = level.RuntimeCameraBaseTranslation;

        if (focusActive)
        {
            const float duration = std::max(0.01f, level.RuntimeCinematicDuration);
            const float elapsed = duration - level.RuntimeCinematicTimer;
            const float rampIn = SmoothStep01(elapsed / 0.12f);
            const float rampOut = SmoothStep01(level.RuntimeCinematicTimer / 0.18f);
            const float focusStrength = rampIn * rampOut;
            Entity focus = scene->GetEntityByUUID(level.RuntimeCinematicFocusEntity);
            if (focus)
            {
                glm::vec3 focusPosition = translation;
                if (focus.HasComponent<SideCombatantComponent>())
                {
                    const auto& combatant = focus.GetComponent<SideCombatantComponent>();
                    focusPosition = {
                        combatant.RuntimeGroundPosition.x,
                        combatant.RuntimeGroundPosition.y +
                            combatant.RuntimeAirHeight +
                            combatant.CollisionHeight * 0.52f,
                        translation.z
                    };
                }
                else if (focus.HasComponent<TransformComponent>())
                {
                    focusPosition = focus.GetComponent<TransformComponent>().Translation;
                }

                translation.x = glm::mix(
                    translation.x,
                    focusPosition.x + level.RuntimeCinematicCameraOffset.x,
                    focusStrength);
                translation.y = glm::mix(
                    translation.y,
                    focusPosition.y + level.RuntimeCinematicCameraOffset.y,
                    focusStrength);

                if (camera.HasComponent<CameraComponent>() &&
                    level.RuntimeCameraProjectionCaptured)
                {
                    auto& cameraComponent = camera.GetComponent<CameraComponent>();
                    if (cameraComponent.Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
                    {
                        const float baseSize = std::max(
                            0.01f,
                            level.RuntimeCameraBaseOrthographicSize);
                        const float focusedSize = baseSize /
                            std::max(1.0f, level.RuntimeCinematicCameraZoom);
                        cameraComponent.Camera.SetOrthographicSize(
                            glm::mix(baseSize, focusedSize, focusStrength));
                    }
                }
            }
        }
        else if (camera.HasComponent<CameraComponent>() &&
            level.RuntimeCameraProjectionCaptured)
        {
            auto& cameraComponent = camera.GetComponent<CameraComponent>();
            if (cameraComponent.Camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
            {
                cameraComponent.Camera.SetOrthographicSize(
                    std::max(0.01f, level.RuntimeCameraBaseOrthographicSize));
            }
        }

        if (shakeActive)
        {
            const float duration = std::max(0.01f, level.RuntimeCameraShakeDuration);
            const float normalized = level.RuntimeCameraShakeTimer / duration;
            const float strength = level.RuntimeCameraShakeStrength * normalized * normalized;
            const float phase = level.RuntimeElapsed * 95.0f;
            translation += glm::vec3(
                std::sin(phase) * strength,
                std::sin(phase * 1.37f + 1.6f) * strength * 0.62f,
                0.0f);
        }

        transform.Translation = translation;
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
