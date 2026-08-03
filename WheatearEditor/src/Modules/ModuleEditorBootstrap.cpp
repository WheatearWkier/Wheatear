#include "wtpch.h"
#include "ModuleEditorBootstrap.h"

#include "Editor/EditorComponentRegistry.h"
#include "Editor/EditorToolRegistry.h"
#include "Panels/EditorCommands.h"
#include "Modules/ArcadeCombat/ArcadeCombatDrawer.h"
#include "Modules/SideCombat/SideCombatDrawer.h"
#include "Modules/SideCombat/SideCombatTuningEditorPanel.h"
#include "Modules/TacticalCombat/TacticalCombatDrawer.h"
#include "Modules/TurnCombat/TurnCombatDrawer.h"
#include "Modules/VisualNovel/VisualNovelDrawer.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Tools/ProjectHealthPanel.h"
#include "Tools/WAOActionEditorPanel.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatComponents.h"
#include "Wheatear/Modules/SideCombat/SideCombatComponents.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatComponents.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatComponents.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelComponents.h"

namespace Wheatear {

    namespace {

        static VisualNovelScriptEditorPanel& GetVisualNovelScriptEditorPanel()
        {
            static VisualNovelScriptEditorPanel panel;
            return panel;
        }

        static SideCombatTuningEditorPanel& GetSideCombatTuningEditorPanel()
        {
            static SideCombatTuningEditorPanel panel;
            return panel;
        }

        static ProjectHealthPanel& GetProjectHealthPanel()
        {
            static ProjectHealthPanel panel;
            return panel;
        }

        static WAOActionEditorPanel& GetWAOActionEditorPanel()
        {
            static WAOActionEditorPanel panel;
            return panel;
        }

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

        static void RegisterEditorTools()
        {
            EditorToolRegistry::Register({
                "Project Health",
                EditorToolCategory::Diagnostics,
                [](const EditorToolContext& context)
                {
                    GetProjectHealthPanel().Open(context);
                },
                []()
                {
                    GetProjectHealthPanel().OnImGuiRender();
                }
            });

            EditorToolRegistry::Register({
                "WAO Action Debugger",
                EditorToolCategory::Gameplay,
                [](const EditorToolContext& context)
                {
                    GetWAOActionEditorPanel().Open(context);
                },
                []()
                {
                    GetWAOActionEditorPanel().OnImGuiRender();
                }
            });

            EditorToolRegistry::Register({
                "VN Script Editor",
                EditorToolCategory::Gameplay,
                [](const EditorToolContext& context)
                {
                    std::string scriptPath = "assets/vn/vertical_slice_intro.vn";
                    if (context.SelectedEntity && context.SelectedEntity.HasComponent<VisualNovelComponent>())
                        scriptPath = context.SelectedEntity.GetComponent<VisualNovelComponent>().ScriptPath;

                    GetVisualNovelScriptEditorPanel().Open(scriptPath);
                },
                []()
                {
                    GetVisualNovelScriptEditorPanel().OnImGuiRender();
                }
            });

            EditorToolRegistry::Register({
                "Side Combat Tuning Editor",
                EditorToolCategory::Gameplay,
                [](const EditorToolContext& context)
                {
                    std::string tuningPath = "side.tuning";
                    if (context.SelectedEntity && context.SelectedEntity.HasComponent<SideCombatLevelComponent>())
                        tuningPath = context.SelectedEntity.GetComponent<SideCombatLevelComponent>().TuningPath;

                    GetSideCombatTuningEditorPanel().Open(tuningPath);
                },
                []()
                {
                    GetSideCombatTuningEditorPanel().OnImGuiRender();
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

        RegisterEditorTools();

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

        RegisterEditorComponent<TurnCombatLevelComponent>(
            "Turn Combat",
            "Turn Combat Level",
            DrawTurnCombatLevelComponent);
        RegisterEditorComponent<TurnCombatantComponent>(
            "Turn Combat",
            "Turn Combatant",
            DrawTurnCombatantComponent);

        RegisterEditorComponent<TacticalCombatLevelComponent>(
            "Tactical Combat",
            "Tactical Combat Level",
            DrawTacticalCombatLevelComponent);
        RegisterEditorComponent<TacticalUnitComponent>(
            "Tactical Combat",
            "Tactical Unit",
            DrawTacticalUnitComponent);
    }

} // namespace Wheatear
