#include "wtpch.h"
#include "ModuleEditorBootstrap.h"

#include "Editor/EditorComponentRegistry.h"
#include "Editor/EditorLocale.h"
#include "Editor/EventScriptGraphPanel.h"
#include "Editor/EditorToolRegistry.h"
#include "Panels/EditorCommands.h"
#include "Modules/ArcadeCombat/ArcadeCombatDrawer.h"
#include "Modules/SideCombat/SideCombatDrawer.h"
#include "Modules/SideCombat/SideCombatHudPresetEditorPanel.h"
#include "Modules/SideCombat/SideCombatTuningEditorPanel.h"
#include "Modules/Progression/ProgressionContentEditorPanel.h"
#include "Modules/TacticalCombat/TacticalCombatDrawer.h"
#include "Modules/TurnCombat/TurnCombatDrawer.h"
#include "Modules/VisualNovel/VisualNovelDrawer.h"
#include "Modules/VisualNovel/VisualNovelScriptEditorPanel.h"
#include "Tools/ProjectHealthPanel.h"
#include "Tools/AssetAliasManifestEditorPanel.h"
#include "Tools/WAOActionEditorPanel.h"
#include "Wheatear/Modules/ArcadeCombat/ArcadeCombatComponents.h"
#include "Wheatear/Modules/SideCombat/SideCombatComponents.h"
#include "Wheatear/Modules/TacticalCombat/TacticalCombatComponents.h"
#include "Wheatear/Modules/TurnCombat/TurnCombatComponents.h"
#include "Wheatear/Modules/VisualNovel/VisualNovelComponents.h"
#include "Wheatear/Scene/Components.h"

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

        static SideCombatHudPresetEditorPanel& GetSideCombatHudPresetEditorPanel()
        {
            static SideCombatHudPresetEditorPanel panel;
            return panel;
        }

        static ProgressionContentEditorPanel& GetProgressionContentEditorPanel()
        {
            static ProgressionContentEditorPanel panel;
            return panel;
        }

        static ProjectHealthPanel& GetProjectHealthPanel()
        {
            static ProjectHealthPanel panel;
            return panel;
        }

        static AssetAliasManifestEditorPanel& GetAssetAliasManifestEditorPanel()
        {
            static AssetAliasManifestEditorPanel panel;
            return panel;
        }

        static WAOActionEditorPanel& GetWAOActionEditorPanel()
        {
            static WAOActionEditorPanel panel;
            return panel;
        }

        static EventScriptGraphPanel& GetEventScriptEditorPanel()
        {
            static EventScriptGraphPanel panel;
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
                "Event Script Editor",
                EditorToolCategory::Gameplay,
                [](const EditorToolContext& context)
                {
                    std::string scriptPath = "assets/events/vertical_slice_flow.wts";
                    std::string eventName;
                    if (context.SelectedEntity && context.SelectedEntity.HasComponent<EventScriptComponent>())
                    {
                        const auto& script = context.SelectedEntity.GetComponent<EventScriptComponent>();
                        if (!script.ScriptPath.empty())
                            scriptPath = script.ScriptPath;
                        eventName = script.StartEvent;
                    }

                    GetEventScriptEditorPanel().Open(scriptPath, eventName);
                },
                []()
                {
                    GetEventScriptEditorPanel().OnImGuiRender();
                }
            });

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
                "Asset Alias / Manifest Editor",
                EditorToolCategory::Gameplay,
                [](const EditorToolContext& context)
                {
                    GetAssetAliasManifestEditorPanel().Open(context);
                },
                []()
                {
                    GetAssetAliasManifestEditorPanel().OnImGuiRender();
                }
            });

            EditorToolRegistry::Register({
                "WAO Action Editor",
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

            EditorToolRegistry::Register({
                "Side Combat HUD Preset Editor",
                EditorToolCategory::Gameplay,
                [](const EditorToolContext& context)
                {
                    std::string presetPath = "side.hud.preset";
                    if (context.SelectedEntity && context.SelectedEntity.HasComponent<SideCombatLevelComponent>())
                        presetPath = context.SelectedEntity.GetComponent<SideCombatLevelComponent>().HudPresetPath;

                    GetSideCombatHudPresetEditorPanel().Open(presetPath);
                },
                []()
                {
                    GetSideCombatHudPresetEditorPanel().OnImGuiRender();
                }
            });

            EditorToolRegistry::Register({
                "Progression Content Editor",
                EditorToolCategory::Gameplay,
                [](const EditorToolContext&)
                {
                    GetProgressionContentEditorPanel().Open();
                },
                []()
                {
                    GetProgressionContentEditorPanel().OnImGuiRender();
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
            EditorLocale::Text("Visual Novel", "视觉小说"),
            EditorLocale::Text("Visual Novel", "视觉小说"),
            DrawVisualNovelComponent);

        RegisterEditorComponent<ArcadeCombatLevelComponent>(
            EditorLocale::Text("Arcade Combat", "街机战斗"),
            EditorLocale::Text("Arcade Combat Level", "街机战斗关卡"),
            DrawArcadeCombatLevelComponent);
        RegisterEditorComponent<ArcadeCombatantComponent>(
            EditorLocale::Text("Arcade Combat", "街机战斗"),
            EditorLocale::Text("Arcade Combatant", "街机战斗单位"),
            DrawArcadeCombatantComponent);
        RegisterEditorComponent<ArcadePlayerControllerComponent>(
            EditorLocale::Text("Arcade Combat", "街机战斗"),
            EditorLocale::Text("Arcade Player Controller", "街机玩家控制器"),
            DrawArcadePlayerControllerComponent);
        RegisterEditorComponent<ArcadeBossComponent>(
            EditorLocale::Text("Arcade Combat", "街机战斗"),
            EditorLocale::Text("Arcade Boss", "街机 Boss"),
            DrawArcadeBossComponent);
        RegisterEditorComponent<ArcadeProjectileComponent>(
            EditorLocale::Text("Arcade Combat", "街机战斗"),
            EditorLocale::Text("Arcade Projectile", "街机子弹"),
            DrawArcadeProjectileComponent);
        RegisterEditorComponent<ArcadeCoverComponent>(
            EditorLocale::Text("Arcade Combat", "街机战斗"),
            EditorLocale::Text("Arcade Cover", "街机掩体"),
            DrawArcadeCoverComponent);
        RegisterEditorComponent<ArcadeTriggerComponent>(
            EditorLocale::Text("Arcade Combat", "街机战斗"),
            EditorLocale::Text("Arcade Trigger", "街机触发器"),
            DrawArcadeTriggerComponent);

        RegisterEditorComponent<SideCombatLevelComponent>(
            EditorLocale::Text("Side Combat", "横版战斗"),
            EditorLocale::Text("Side Combat Level", "横版战斗关卡"),
            DrawSideCombatLevelComponent);
        RegisterEditorComponent<SideCombatantComponent>(
            EditorLocale::Text("Side Combat", "横版战斗"),
            EditorLocale::Text("Side Combatant", "横版战斗单位"),
            DrawSideCombatantComponent);
        RegisterEditorComponent<SidePlayerControllerComponent>(
            EditorLocale::Text("Side Combat", "横版战斗"),
            EditorLocale::Text("Side Player Controller", "横版玩家控制器"),
            DrawSidePlayerControllerComponent);
        RegisterEditorComponent<SideEnemyAIComponent>(
            EditorLocale::Text("Side Combat", "横版战斗"),
            EditorLocale::Text("Side Enemy AI", "横版敌人 AI"),
            DrawSideEnemyAIComponent);
        RegisterEditorComponent<SideHitboxComponent>(
            EditorLocale::Text("Side Combat", "横版战斗"),
            EditorLocale::Text("Side Hitbox", "横版命中框"),
            DrawSideHitboxComponent);
        RegisterEditorComponent<SidePickupComponent>(
            EditorLocale::Text("Side Combat", "横版战斗"),
            EditorLocale::Text("Side Pickup", "横版拾取物"),
            DrawSidePickupComponent);

        RegisterEditorComponent<TurnCombatLevelComponent>(
            EditorLocale::Text("Turn Combat", "回合制战斗"),
            EditorLocale::Text("Turn Combat Level", "回合制战斗关卡"),
            DrawTurnCombatLevelComponent);
        RegisterEditorComponent<TurnCombatantComponent>(
            EditorLocale::Text("Turn Combat", "回合制战斗"),
            EditorLocale::Text("Turn Combatant", "回合制战斗单位"),
            DrawTurnCombatantComponent);

        RegisterEditorComponent<TacticalCombatLevelComponent>(
            EditorLocale::Text("Tactical Combat", "战棋战斗"),
            EditorLocale::Text("Tactical Combat Level", "战棋战斗关卡"),
            DrawTacticalCombatLevelComponent);
        RegisterEditorComponent<TacticalUnitComponent>(
            EditorLocale::Text("Tactical Combat", "战棋战斗"),
            EditorLocale::Text("Tactical Unit", "战棋单位"),
            DrawTacticalUnitComponent);
    }

} // namespace Wheatear
