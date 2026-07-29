#include "wtpch.h"
#include "ModuleEditorBootstrap.h"

#include "Editor/EditorComponentRegistry.h"
#include "Panels/EditorCommands.h"
#include "Modules/ArcadeCombat/ArcadeCombatDrawer.h"
#include "Modules/SideCombat/SideCombatDrawer.h"
#include "Modules/VisualNovel/VisualNovelDrawer.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatComponents.h"
#include "Wheatear/Modules/SideCombat/SideCombatComponents.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelComponents.h"

namespace Wheatear {

    namespace {

        template<typename T>
        void RegisterEditorComponent(const char* category,
            const char* label,
            void (*draw)(Entity))
        {
            EditorComponentRegistry::Register({
                category,
                label,
                [](Entity entity)
                {
                    return entity && !entity.HasComponent<T>();
                },
                [](Entity entity)
                {
                    if (entity)
                    {
                        auto command = std::make_unique<AddComponentCommand<T>>(entity);
                        command->Execute();
                        CommandHistory::Get().Push(std::move(command));
                    }
                },
                [draw](Entity entity)
                {
                    draw(entity);
                }
            });
        }

    } // namespace

    void RegisterDefaultGameplayEditorModules()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;

        RegisterEditorComponent<VisualNovelComponent>(
            "Visual Novel",
            "Visual Novel",
            DrawVisualNovelComponent);

        RegisterEditorComponent<ArcadeCombatLevelComponent>(
            "Arcade Combat",
            "Arcade Combat Level",
            DrawArcadeCombatLevelComponent);
        RegisterEditorComponent<ArcadeCombatantComponent>(
            "Arcade Combat",
            "Arcade Combatant",
            DrawArcadeCombatantComponent);
        RegisterEditorComponent<ArcadePlayerControllerComponent>(
            "Arcade Combat",
            "Arcade Player Controller",
            DrawArcadePlayerControllerComponent);
        RegisterEditorComponent<ArcadeBossComponent>(
            "Arcade Combat",
            "Arcade Boss",
            DrawArcadeBossComponent);
        RegisterEditorComponent<ArcadeProjectileComponent>(
            "Arcade Combat",
            "Arcade Projectile",
            DrawArcadeProjectileComponent);
        RegisterEditorComponent<ArcadeCoverComponent>(
            "Arcade Combat",
            "Arcade Cover",
            DrawArcadeCoverComponent);
        RegisterEditorComponent<ArcadeTriggerComponent>(
            "Arcade Combat",
            "Arcade Trigger",
            DrawArcadeTriggerComponent);

        RegisterEditorComponent<SideCombatLevelComponent>(
            "Side Combat",
            "Side Combat Level",
            DrawSideCombatLevelComponent);
        RegisterEditorComponent<SideCombatantComponent>(
            "Side Combat",
            "Side Combatant",
            DrawSideCombatantComponent);
        RegisterEditorComponent<SidePlayerControllerComponent>(
            "Side Combat",
            "Side Player Controller",
            DrawSidePlayerControllerComponent);
        RegisterEditorComponent<SideEnemyAIComponent>(
            "Side Combat",
            "Side Enemy AI",
            DrawSideEnemyAIComponent);
        RegisterEditorComponent<SideHitboxComponent>(
            "Side Combat",
            "Side Hitbox",
            DrawSideHitboxComponent);
        RegisterEditorComponent<SidePickupComponent>(
            "Side Combat",
            "Side Pickup",
            DrawSidePickupComponent);
    }

} // namespace Wheatear
