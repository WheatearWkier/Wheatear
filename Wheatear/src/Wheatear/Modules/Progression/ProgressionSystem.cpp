#include "wtpch.h"
#include "ProgressionSystem.h"

#include "GameProgress.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"

namespace Wheatear {

    namespace {

        static Entity FindEntityByName(Scene* scene, const std::string& name)
        {
            if (!scene || name.empty())
                return {};

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TagComponent>())
            {
                if (registry.get<TagComponent>(e).Tag == name)
                    return { e, scene };
            }
            return {};
        }

        static bool HasEntity(Scene* scene, const std::string& name)
        {
            return static_cast<bool>(FindEntityByName(scene, name));
        }

        static void SetText(Scene* scene, const std::string& entityName, const std::string& text)
        {
            Entity entity = FindEntityByName(scene, entityName);
            if (entity && entity.HasComponent<UITextComponent>())
                entity.GetComponent<UITextComponent>().Text = text;
        }

        static void UpdateHub(Scene* scene)
        {
            if (!HasEntity(scene, "Hub_Status"))
                return;

            SetText(scene, "Hub_Subtitle", GameProgress::BuildHubSubtitle());
            SetText(scene, "Hub_Status", GameProgress::BuildHubStatus());
            SetText(scene, "Hub_Button_Dungeon", GameProgress::GetDungeonButtonText());
            SetText(scene, "Hub_Button_Skill", GameProgress::GetSkillButtonText());
            SetText(scene, "Hub_Button_Equip", GameProgress::GetEquipmentButtonText());
        }

    } // namespace

    void ProgressionSystem::OnRuntimeStart(Scene* scene)
    {
        UpdateHub(scene);
    }

    void ProgressionSystem::OnUpdateRuntime(Scene* scene, Timestep)
    {
        UpdateHub(scene);
    }

} // namespace Wheatear
