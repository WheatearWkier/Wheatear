#include "wtpch.h"
#include "RTSAnimationSystem.h"

#include "RTSComponents.h"

#include "Wheatear/Renderer/Model.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Scene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace Wheatear {

    namespace {

        bool EqualsIgnoreCase(const std::string& left, const std::string& right)
        {
            if (left.size() != right.size())
                return false;
            for (size_t i = 0; i < left.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(left[i]))
                    != std::tolower(static_cast<unsigned char>(right[i])))
                {
                    return false;
                }
            }
            return true;
        }

        int FindClipIndex(const Model& model,
            const std::string& name,
            size_t fallback)
        {
            const auto& animations = model.GetAnimations();
            if (animations.empty())
                return -1;

            for (size_t i = 0; i < animations.size(); ++i)
            {
                if (EqualsIgnoreCase(animations[i].Name, name))
                    return static_cast<int>(i);
            }

            return static_cast<int>(std::min(fallback, animations.size() - 1));
        }

        int ClipForState(const RTSUnitComponent& unit,
            const Model& model,
            RTSUnitState state)
        {
            const size_t count = model.GetAnimations().size();
            switch (state)
            {
            case RTSUnitState::Move:
                return FindClipIndex(model, unit.MoveClip, count > 1 ? 1 : 0);
            case RTSUnitState::Attack:
                return FindClipIndex(model, unit.AttackClip, count > 2 ? 2 : 0);
            case RTSUnitState::Dying:
                return FindClipIndex(model, unit.DeathClip, count - 1);
            case RTSUnitState::Idle:
            default:
                return FindClipIndex(model, unit.IdleClip, 0);
            }
        }

        void ApplyGreyboxState(TransformComponent& transform,
            const RTSUnitComponent& unit)
        {
            float factor = 1.0f;
            switch (unit.State)
            {
            case RTSUnitState::Move:
                factor = 1.0f + 0.035f * std::sin(unit.StateTime * 12.0f);
                break;
            case RTSUnitState::Attack:
                factor = 1.0f + 0.18f * std::max(
                    0.0f, 1.0f - unit.StateTime / 0.18f);
                break;
            case RTSUnitState::Dying:
            {
                const float duration = std::max(unit.DeathDuration, 0.01f);
                factor = glm::clamp(1.0f - unit.StateTime / duration,
                    0.0f, 1.0f);
                break;
            }
            case RTSUnitState::Idle:
            default:
                factor = 1.0f + 0.018f * std::sin(unit.StateTime * 2.0f);
                break;
            }

            transform.Scale = unit.VisualScale * factor;
        }

    } // namespace

    void RTSAnimationSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        (void)ts;
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<RTSUnitComponent, TransformComponent>())
        {
            const auto& unit = registry.get<RTSUnitComponent>(e);
            auto& transform = registry.get<TransformComponent>(e);

            // Buildings are animated by the construction/production system.
            if (unit.Type == RTSUnitType::Building)
                continue;

            auto* renderer = registry.try_get<MeshRendererComponent>(e);
            auto* animator = registry.try_get<Animator3DComponent>(e);
            const bool hasSkeletalModel = renderer && renderer->Model
                && renderer->Model->HasSkin()
                && !renderer->Model->GetAnimations().empty();

            if (hasSkeletalModel && animator)
            {
                const Model& model = *renderer->Model;
                const int clip = ClipForState(unit, model, unit.State);
                if (clip >= 0)
                {
                    // Death is a one-shot; the unit is removed shortly after.
                    animator->Loop = unit.State != RTSUnitState::Dying;
                    const float fade = unit.State == RTSUnitState::Dying
                        ? 0.08f
                        : (unit.State == RTSUnitState::Attack ? 0.08f : 0.18f);
                    animator->PlayClip(clip, fade);
                }

                // Models keep their authored scale; the state is carried by
                // the skeletal clip.
                continue;
            }

            ApplyGreyboxState(transform, unit);
        }
    }

} // namespace Wheatear
