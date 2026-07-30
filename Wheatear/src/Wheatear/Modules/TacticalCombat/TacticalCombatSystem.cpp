#include "wtpch.h"
#include "TacticalCombatSystem.h"

#include "TacticalCombatComponents.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace Wheatear {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetImageAlpha;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetImageTexture;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        enum class TacticalTargetRule
        {
            Enemy = 0,
            Ally = 1,
            Self = 2
        };

        struct TacticalSkillDefinition
        {
            const char* Id = "";
            const char* DisplayName = "";
            const char* Description = "";
            const char* IconPath = "";
            const char* SoundPath = "";
            const char* EffectFramePattern = "";
            int EffectFrameCount = 1;
            float EffectFrameRate = 12.0f;
            TacticalTargetRule TargetRule = TacticalTargetRule::Enemy;
            int Range = 1;
            float Power = 1.0f;
            float HealPower = 0.0f;
            float DefensePierce = 0.0f;
            bool Magic = false;
            bool Guard = false;
        };

        static const std::vector<TacticalSkillDefinition>& SkillLibrary()
        {
            static const std::vector<TacticalSkillDefinition> skills = {
                {
                    "sword_slash",
                    "魔剑斩",
                    "近身单体斩击。用于预览战旗近战职业的基础行动。",
                    "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_attack.png",
                    "assets/vertical_slice/tactical_combat/audio/tac_slash.wav",
                    "assets/vertical_slice/tactical_combat/effects/vfx_tac_slash_{frame2}.png",
                    5,
                    14.0f,
                    TacticalTargetRule::Enemy,
                    1,
                    1.00f,
                    0.0f,
                    0.05f,
                    false,
                    false
                },
                {
                    "aether_lance",
                    "灵枪",
                    "三格射程的魔法突刺。让假玩法也能体现走位和射程差。",
                    "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_magic.png",
                    "assets/vertical_slice/tactical_combat/audio/tac_magic.wav",
                    "assets/vertical_slice/tactical_combat/effects/vfx_tac_magic_{frame2}.png",
                    6,
                    14.0f,
                    TacticalTargetRule::Enemy,
                    3,
                    1.25f,
                    0.0f,
                    0.22f,
                    true,
                    false
                },
                {
                    "white_pulse",
                    "白脉",
                    "治疗三格内的我方单位。用于预览战旗辅助职业。",
                    "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_heal.png",
                    "assets/vertical_slice/tactical_combat/audio/tac_heal.wav",
                    "assets/vertical_slice/tactical_combat/effects/vfx_tac_heal_{frame2}.png",
                    6,
                    12.0f,
                    TacticalTargetRule::Ally,
                    3,
                    0.0f,
                    1.05f,
                    0.0f,
                    true,
                    false
                },
                {
                    "guard_wait",
                    "守备",
                    "结束行动并进入防御姿态。",
                    "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_guard.png",
                    "assets/vertical_slice/tactical_combat/audio/tac_guard.wav",
                    "assets/vertical_slice/tactical_combat/effects/vfx_tac_guard_{frame2}.png",
                    4,
                    10.0f,
                    TacticalTargetRule::Self,
                    0,
                    0.0f,
                    0.0f,
                    0.0f,
                    false,
                    true
                },
                {
                    "enemy_strike",
                    "敌兵斩",
                    "敌方近身攻击。",
                    "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_enemy.png",
                    "assets/vertical_slice/tactical_combat/audio/tac_hit.wav",
                    "assets/vertical_slice/tactical_combat/effects/vfx_tac_slash_{frame2}.png",
                    5,
                    14.0f,
                    TacticalTargetRule::Enemy,
                    1,
                    0.95f,
                    0.0f,
                    0.0f,
                    false,
                    false
                },
                {
                    "enemy_dark",
                    "暗术",
                    "敌方法师远程魔法。",
                    "assets/vertical_slice/tactical_combat/ui/icons/cmd_tac_enemy_magic.png",
                    "assets/vertical_slice/tactical_combat/audio/tac_magic.wav",
                    "assets/vertical_slice/tactical_combat/effects/vfx_tac_dark_{frame2}.png",
                    6,
                    14.0f,
                    TacticalTargetRule::Enemy,
                    3,
                    1.12f,
                    0.0f,
                    0.18f,
                    true,
                    false
                }
            };
            return skills;
        }

        static const TacticalSkillDefinition* FindSkill(const std::string& id)
        {
            const auto& skills = SkillLibrary();
            auto it = std::find_if(skills.begin(), skills.end(),
                [&](const TacticalSkillDefinition& skill) { return skill.Id == id; });
            return it == skills.end() ? nullptr : &(*it);
        }

        static std::vector<std::string> SplitCommand(const std::string& command)
        {
            std::vector<std::string> parts;
            size_t start = 0;
            while (start <= command.size())
            {
                const size_t separator = command.find(':', start);
                if (separator == std::string::npos)
                {
                    parts.push_back(command.substr(start));
                    break;
                }

                parts.push_back(command.substr(start, separator - start));
                start = separator + 1;
            }
            return parts;
        }

        static void ReplaceAll(std::string& value, const std::string& from, const std::string& to)
        {
            if (from.empty())
                return;

            size_t position = 0;
            while ((position = value.find(from, position)) != std::string::npos)
            {
                value.replace(position, from.size(), to);
                position += to.size();
            }
        }

        static std::string FormatFramePath(const std::string& pattern, int frameIndex)
        {
            if (pattern.empty())
                return {};

            const int frame = std::max(frameIndex + 1, 1);
            std::ostringstream padded;
            padded << std::setw(2) << std::setfill('0') << frame;

            std::string path = pattern;
            ReplaceAll(path, "{frame2}", padded.str());
            ReplaceAll(path, "{frame}", std::to_string(frame));
            return path;
        }

        static std::string CellTag(const TacticalCombatLevelComponent& level, int x, int y)
        {
            return level.CellEntityPrefix + std::to_string(x) + "_" + std::to_string(y);
        }

        static glm::vec2 CellTopLeft(const TacticalCombatLevelComponent& level, int x, int y)
        {
            return {
                level.BoardOrigin.x + (float)x * level.CellSize.x,
                level.BoardOrigin.y + (float)y * level.CellSize.y
            };
        }

        static glm::vec2 CellCenter(const TacticalCombatLevelComponent& level, int x, int y)
        {
            return CellTopLeft(level, x, y) + level.CellSize * 0.5f;
        }

        static int Distance(int ax, int ay, int bx, int by)
        {
            return std::abs(ax - bx) + std::abs(ay - by);
        }

        static bool InBounds(const TacticalCombatLevelComponent& level, int x, int y)
        {
            return x >= 0 && y >= 0 && x < level.GridWidth && y < level.GridHeight;
        }

        static void PlayTacticalSound(const std::string& path, float volume = 0.48f)
        {
            if (path.empty())
                return;

            const auto& settings = GameProgress::GetState().Settings;
            const float master = AudioEngine::PercentToGain(static_cast<float>(settings.MasterVolume));
            const float sfx = AudioEngine::PercentToGain(static_cast<float>(settings.SFXVolume));
            AudioEngine::PlaySound(path, volume * master * sfx);
        }

        static std::vector<Entity> CollectUnits(Scene* scene)
        {
            std::vector<Entity> units;
            if (!scene)
                return units;

            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<TacticalUnitComponent>())
                units.emplace_back(entity, scene);

            std::sort(units.begin(), units.end(), [](Entity a, Entity b)
            {
                const auto& ac = a.GetComponent<TacticalUnitComponent>();
                const auto& bc = b.GetComponent<TacticalUnitComponent>();
                if (ac.Team != bc.Team)
                    return ac.Team < bc.Team;
                return ac.Slot < bc.Slot;
            });
            return units;
        }

        static Entity FindUnitAt(Scene* scene, int x, int y)
        {
            for (Entity unit : CollectUnits(scene))
            {
                const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                if (tactical.RuntimeAlive && tactical.GridX == x && tactical.GridY == y)
                    return unit;
            }
            return {};
        }

        static bool HasAliveTeam(Scene* scene, int team)
        {
            for (Entity unit : CollectUnits(scene))
            {
                const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                if (tactical.Team == team && tactical.RuntimeAlive)
                    return true;
            }
            return false;
        }

        static bool AllPlayerUnitsDone(Scene* scene)
        {
            bool hasAlivePlayer = false;
            for (Entity unit : CollectUnits(scene))
            {
                const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                if (tactical.Team != (int)TacticalCombatTeam::Player || !tactical.RuntimeAlive)
                    continue;
                hasAlivePlayer = true;
                if (!tactical.RuntimeHasActed)
                    return false;
            }
            return hasAlivePlayer;
        }

        static bool IsValidTarget(
            const TacticalSkillDefinition& skill,
            const TacticalUnitComponent& actor,
            const TacticalUnitComponent& target)
        {
            if (!target.RuntimeAlive)
                return false;

            if (skill.TargetRule == TacticalTargetRule::Self)
                return actor.GridX == target.GridX && actor.GridY == target.GridY;

            if (skill.TargetRule == TacticalTargetRule::Enemy && actor.Team == target.Team)
                return false;
            if (skill.TargetRule == TacticalTargetRule::Ally && actor.Team != target.Team)
                return false;

            return Distance(actor.GridX, actor.GridY, target.GridX, target.GridY) <= skill.Range;
        }

        static bool CanMoveTo(Scene* scene,
            const TacticalCombatLevelComponent& level,
            const TacticalUnitComponent& unit,
            int x,
            int y)
        {
            if (!InBounds(level, x, y))
                return false;
            if (unit.RuntimeMoved)
                return false;
            if (Distance(unit.GridX, unit.GridY, x, y) > unit.MoveRange)
                return false;

            Entity occupant = FindUnitAt(scene, x, y);
            return !occupant;
        }

        static std::string SelectVisualClip(
            const TacticalUnitComponent& unit,
            const TacticalCombatLevelComponent& level,
            Entity entity)
        {
            if (!unit.RuntimeAlive)
                return "down";
            if (unit.RuntimeHitFlashTimer > 0.0f && !unit.HitFramePattern.empty())
                return "hit";
            if (level.RuntimePhase == TacticalCombatPhase::Acting
                && entity
                && entity.GetName() == level.RuntimeActionActorTag
                && !unit.AttackFramePattern.empty())
            {
                return "attack";
            }
            return "idle";
        }

        static const std::string& PatternForClip(const TacticalUnitComponent& unit, const std::string& clip)
        {
            if (clip == "attack")
                return unit.AttackFramePattern;
            if (clip == "hit")
                return unit.HitFramePattern;
            if (clip == "down")
                return unit.DownFramePattern;
            return unit.IdleFramePattern;
        }

        static int FrameCountForClip(const TacticalUnitComponent& unit, const std::string& clip)
        {
            if (clip == "attack")
                return unit.AttackFrameCount;
            if (clip == "hit")
                return unit.HitFrameCount;
            if (clip == "down")
                return unit.DownFrameCount;
            return unit.IdleFrameCount;
        }

        static void UpdateUnitVisual(Scene* scene,
            const TacticalCombatLevelComponent& level,
            Entity entity,
            TacticalUnitComponent& unit,
            float dt)
        {
            if (!entity)
                return;

            const glm::vec2 topLeft = CellTopLeft(level, unit.GridX, unit.GridY)
                + glm::vec2(level.CellSize.x * 0.10f, level.CellSize.y * 0.02f);
            SetWidgetTopLeft(scene, entity.GetName(), topLeft, level.CellSize * glm::vec2(0.80f, 0.92f));

            const std::string clip = SelectVisualClip(unit, level, entity);
            if (unit.RuntimeVisualClip != clip)
            {
                unit.RuntimeVisualClip = clip;
                unit.RuntimeVisualTimer = 0.0f;
            }
            else
            {
                unit.RuntimeVisualTimer += dt;
            }

            const std::string& pattern = PatternForClip(unit, clip);
            const int frameCount = std::max(1, FrameCountForClip(unit, clip));
            if (!pattern.empty())
            {
                int frameIndex = 0;
                if (clip == "attack" && level.RuntimePhase == TacticalCombatPhase::Acting)
                {
                    const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 0.999f);
                    frameIndex = std::min(frameCount - 1, (int)(t * (float)frameCount));
                }
                else if (clip == "idle")
                {
                    frameIndex = (int)(unit.RuntimeVisualTimer * std::max(1.0f, unit.AnimationFrameRate)) % frameCount;
                }
                else
                {
                    frameIndex = std::min(frameCount - 1,
                        (int)(unit.RuntimeVisualTimer * std::max(1.0f, unit.AnimationFrameRate)));
                }
                SetImageTexture(scene, entity.GetName(), FormatFramePath(pattern, frameIndex), false);
            }

            glm::vec4 tint = { 1.0f, 1.0f, 1.0f, unit.RuntimeAlive ? 1.0f : 0.42f };
            if (unit.RuntimeHasActed && unit.RuntimeAlive)
                tint = { 0.56f, 0.62f, 0.68f, 0.86f };
            if (unit.RuntimeHitFlashTimer > 0.0f)
                tint = unit.Team == (int)TacticalCombatTeam::Player
                    ? glm::vec4{ 0.72f, 1.0f, 0.82f, 1.0f }
                    : glm::vec4{ 1.0f, 0.62f, 0.52f, 1.0f };
            SetImageColor(scene, entity.GetName(), tint);

            if (!unit.MarkerEntityName.empty())
            {
                const bool selected = entity.GetName() == level.RuntimeSelectedUnitTag;
                SetWidgetVisible(scene, unit.MarkerEntityName, selected);
                SetWidgetTopLeft(scene, unit.MarkerEntityName,
                    CellTopLeft(level, unit.GridX, unit.GridY) + level.CellSize * 0.05f,
                    level.CellSize * 0.90f);
            }
        }

        static void UpdateTiles(Scene* scene, TacticalCombatLevelComponent& level)
        {
            Entity selected = FindEntityByName(scene, level.RuntimeSelectedUnitTag);
            const TacticalUnitComponent* selectedUnit = selected && selected.HasComponent<TacticalUnitComponent>()
                ? &selected.GetComponent<TacticalUnitComponent>()
                : nullptr;
            const TacticalSkillDefinition* selectedSkill = FindSkill(level.RuntimeSelectedSkillId);

            for (int y = 0; y < level.GridHeight; ++y)
            {
                for (int x = 0; x < level.GridWidth; ++x)
                {
                    glm::vec4 color = level.TileNormalColor;
                    if (selectedUnit)
                    {
                        if (x == selectedUnit->GridX && y == selectedUnit->GridY)
                        {
                            color = level.TileSelectedColor;
                        }
                        else if (level.RuntimePhase == TacticalCombatPhase::Targeting && selectedSkill)
                        {
                            Entity occupant = FindUnitAt(scene, x, y);
                            if (occupant && IsValidTarget(*selectedSkill, *selectedUnit, occupant.GetComponent<TacticalUnitComponent>()))
                                color = selectedSkill->TargetRule == TacticalTargetRule::Ally ? level.TileMoveColor : level.TileAttackColor;
                        }
                        else if (level.RuntimePhase == TacticalCombatPhase::AwaitCommand)
                        {
                            Entity occupant = FindUnitAt(scene, x, y);
                            if (!occupant && CanMoveTo(scene, level, *selectedUnit, x, y))
                                color = level.TileMoveColor;
                            else if (occupant && occupant.GetComponent<TacticalUnitComponent>().Team != selectedUnit->Team
                                && Distance(selectedUnit->GridX, selectedUnit->GridY, x, y) <= selectedUnit->AttackRange)
                            {
                                color = level.TileAttackColor;
                            }
                        }
                    }

                    SetImageColor(scene, CellTag(level, x, y), color);
                }
            }
        }

        static void UpdateStatusUI(Scene* scene, TacticalCombatLevelComponent& level)
        {
            for (Entity unitEntity : CollectUnits(scene))
            {
                auto& unit = unitEntity.GetComponent<TacticalUnitComponent>();
                SetProgress(scene, unit.HealthBarEntityName, unit.Health, unit.MaxHealth);

                std::ostringstream status;
                status << unit.DisplayName << " " << (int)unit.Health << "/" << (int)unit.MaxHealth;
                if (unit.RuntimeGuarding)
                    status << " 守备";
                if (unit.RuntimeHasActed && unit.RuntimeAlive)
                    status << " 行动完";
                if (!unit.RuntimeAlive)
                    status << " 倒下";
                SetText(scene, unit.StatusTextEntityName, status.str());
            }

            std::string phase = "战旗演示";
            switch (level.RuntimePhase)
            {
            case TacticalCombatPhase::Intro: phase = "准备"; break;
            case TacticalCombatPhase::PlayerTurn: phase = "我方回合"; break;
            case TacticalCombatPhase::AwaitCommand: phase = "选择行动"; break;
            case TacticalCombatPhase::Targeting: phase = "选择目标"; break;
            case TacticalCombatPhase::Acting: phase = "演出中"; break;
            case TacticalCombatPhase::EnemyTurn: phase = "敌方回合"; break;
            case TacticalCombatPhase::Victory: phase = "胜利"; break;
            case TacticalCombatPhase::Defeat: phase = "失败"; break;
            }
            SetText(scene, level.PhaseTextEntityName, phase + "  第 " + std::to_string(level.RuntimeRound) + " 回合");
            SetText(scene, level.MessageTextEntityName, level.RuntimeMessage);
        }

        static std::optional<std::string> ResolvePlayerSkillId(const TacticalUnitComponent& unit, const std::string& slot)
        {
            if (slot == "slot0" || slot == "basic")
                return unit.BasicSkillId;
            if (slot == "slot1")
                return unit.Skill1Id;
            if (slot == "slot2")
                return unit.Skill2Id;
            if (slot == "guard" || slot == "wait")
                return std::string("guard_wait");
            return {};
        }

        static void UpdateCommandUI(Scene* scene, TacticalCombatLevelComponent& level)
        {
            const bool visible = level.RuntimePhase == TacticalCombatPhase::AwaitCommand
                || level.RuntimePhase == TacticalCombatPhase::Targeting;
            SetWidgetVisible(scene, level.CommandPanelEntityName, visible);

            Entity selected = FindEntityByName(scene, level.RuntimeSelectedUnitTag);
            if (!selected || !selected.HasComponent<TacticalUnitComponent>())
            {
                SetText(scene, level.DetailTextEntityName, "选择我方单位。");
                return;
            }

            const auto& unit = selected.GetComponent<TacticalUnitComponent>();
            const std::string skillSlots[] = {
                unit.BasicSkillId,
                unit.Skill1Id,
                unit.Skill2Id,
                "guard_wait"
            };

            for (int i = 0; i < 4; ++i)
            {
                const std::string prefix = "TK_Command_" + std::to_string(i + 1);
                const TacticalSkillDefinition* skill = FindSkill(skillSlots[i]);
                SetWidgetVisible(scene, prefix + "_Root", visible && skill);
                if (!skill)
                    continue;

                SetImageTexture(scene, prefix + "_Icon", skill->IconPath, true);
                SetText(scene, prefix + "_Text", skill->DisplayName);
            }

            const TacticalSkillDefinition* selectedSkill = FindSkill(level.RuntimeSelectedSkillId.empty()
                ? unit.BasicSkillId
                : level.RuntimeSelectedSkillId);
            if (selectedSkill)
            {
                std::ostringstream detail;
                detail << unit.DisplayName << " / " << unit.ClassName << "\n"
                    << selectedSkill->DisplayName << "  射程 " << selectedSkill->Range << "\n"
                    << selectedSkill->Description;
                SetText(scene, level.DetailTextEntityName, detail.str());
            }
        }

        static Entity FindNearestAliveEnemy(Scene* scene, const TacticalUnitComponent& actor)
        {
            Entity best;
            int bestDistance = 999;
            for (Entity unit : CollectUnits(scene))
            {
                const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                if (!tactical.RuntimeAlive || tactical.Team == actor.Team)
                    continue;

                const int distance = Distance(actor.GridX, actor.GridY, tactical.GridX, tactical.GridY);
                if (distance < bestDistance)
                {
                    best = unit;
                    bestDistance = distance;
                }
            }
            return best;
        }

        static void BeginAction(
            Scene* scene,
            TacticalCombatLevelComponent& level,
            Entity actor,
            const std::string& skillId,
            Entity target,
            TacticalCombatPhase returnPhase)
        {
            if (!actor || !actor.HasComponent<TacticalUnitComponent>())
                return;

            level.RuntimePhase = TacticalCombatPhase::Acting;
            level.RuntimeActionReturnPhase = returnPhase;
            level.RuntimeActionActorTag = actor.GetName();
            level.RuntimeActionTargetTag = target ? target.GetName() : actor.GetName();
            level.RuntimeActionSkillId = skillId;
            level.RuntimeActionTimer = 0.0f;
            level.RuntimeActionApplied = false;

            const auto& unit = actor.GetComponent<TacticalUnitComponent>();
            const TacticalSkillDefinition* skill = FindSkill(skillId);
            level.RuntimeMessage = unit.DisplayName + " 使用 " + (skill ? skill->DisplayName : skillId) + "。";
            if (skill)
                PlayTacticalSound(skill->SoundPath, 0.50f);
        }

        static void FinishSelectedPlayerAction(Scene* scene, TacticalCombatLevelComponent& level)
        {
            Entity selected = FindEntityByName(scene, level.RuntimeSelectedUnitTag);
            if (selected && selected.HasComponent<TacticalUnitComponent>())
            {
                auto& unit = selected.GetComponent<TacticalUnitComponent>();
                unit.RuntimeHasActed = true;
                unit.RuntimeGuarding = level.RuntimeSelectedSkillId == "guard_wait";
            }

            level.RuntimeSelectedUnitTag.clear();
            level.RuntimeSelectedSkillId.clear();

            if (AllPlayerUnitsDone(scene))
            {
                level.RuntimePhase = TacticalCombatPhase::EnemyTurn;
                level.RuntimeEnemyCursor = 0;
                level.RuntimeEnemyStepTimer = 0.0f;
                level.RuntimeMessage = "敌方回合。";
            }
            else
            {
                level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
                level.RuntimeMessage = "选择一个尚未行动的我方单位。";
            }
        }

        static void ApplyAction(Scene* scene, TacticalCombatLevelComponent& level)
        {
            Entity actor = FindEntityByName(scene, level.RuntimeActionActorTag);
            Entity target = FindEntityByName(scene, level.RuntimeActionTargetTag);
            const TacticalSkillDefinition* skill = FindSkill(level.RuntimeActionSkillId);
            if (!actor || !target || !skill
                || !actor.HasComponent<TacticalUnitComponent>()
                || !target.HasComponent<TacticalUnitComponent>())
            {
                return;
            }

            auto& actorUnit = actor.GetComponent<TacticalUnitComponent>();
            auto& targetUnit = target.GetComponent<TacticalUnitComponent>();

            if (skill->Guard)
            {
                actorUnit.RuntimeGuarding = true;
                level.RuntimeMessage = actorUnit.DisplayName + " 进入守备姿态。";
                return;
            }

            if (skill->HealPower > 0.0f)
            {
                const float amount = std::max(8.0f, actorUnit.Magic * skill->HealPower);
                targetUnit.Health = std::min(targetUnit.MaxHealth, targetUnit.Health + amount);
                targetUnit.RuntimeHitFlashTimer = 0.25f;
                level.RuntimeMessage = targetUnit.DisplayName + " 回复 " + std::to_string((int)amount) + " 点生命。";
                return;
            }

            const float offense = skill->Magic ? actorUnit.Magic : actorUnit.Attack;
            const float defense = targetUnit.Defense * (skill->Magic ? 0.45f : 1.0f) * (1.0f - skill->DefensePierce);
            float damage = std::max(6.0f, offense * skill->Power - defense);
            if (targetUnit.RuntimeGuarding)
                damage *= 0.55f;
            if (targetUnit.Invulnerable)
                damage = 0.0f;

            targetUnit.Health = std::max(0.0f, targetUnit.Health - damage);
            targetUnit.RuntimeHitFlashTimer = 0.30f;
            targetUnit.RuntimeGuarding = false;
            if (targetUnit.Health <= 0.0f)
                targetUnit.RuntimeAlive = false;

            PlayTacticalSound("assets/vertical_slice/tactical_combat/audio/tac_hit.wav", 0.44f);
            level.RuntimeMessage = actorUnit.DisplayName + " 对 " + targetUnit.DisplayName
                + " 造成 " + std::to_string((int)damage) + " 点伤害。";
        }

        static void UpdateActionEffect(Scene* scene, TacticalCombatLevelComponent& level)
        {
            const TacticalSkillDefinition* skill = FindSkill(level.RuntimeActionSkillId);
            Entity target = FindEntityByName(scene, level.RuntimeActionTargetTag);
            if (!skill || !target || !target.HasComponent<TacticalUnitComponent>())
            {
                SetImageAlpha(scene, level.ActionEffectEntityName, 0.0f);
                return;
            }

            const auto& targetUnit = target.GetComponent<TacticalUnitComponent>();
            const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 1.0f);
            const float alpha = std::max(0.0f, std::sin(t * 3.14159265f));
            const int frameIndex = std::min(std::max(skill->EffectFrameCount, 1) - 1,
                (int)(t * (float)std::max(skill->EffectFrameCount, 1)));

            SetWidgetTopLeft(scene,
                level.ActionEffectEntityName,
                CellTopLeft(level, targetUnit.GridX, targetUnit.GridY) - level.CellSize * 0.22f,
                level.CellSize * 1.44f);
            SetImageTexture(scene, level.ActionEffectEntityName, FormatFramePath(skill->EffectFramePattern, frameIndex), true);
            SetImageColor(scene, level.ActionEffectEntityName, { 1.0f, 1.0f, 1.0f, alpha });
            SetWidgetVisible(scene, level.ActionEffectEntityName, alpha > 0.02f);
        }

        static void EndAction(Scene* scene, TacticalCombatLevelComponent& level)
        {
            SetImageAlpha(scene, level.ActionEffectEntityName, 0.0f);

            if (!HasAliveTeam(scene, (int)TacticalCombatTeam::Enemy))
            {
                level.RuntimePhase = TacticalCombatPhase::Victory;
                level.RuntimeResultTimer = 0.0f;
                level.RuntimeMessage = "战旗假玩法胜利。";
                PlayTacticalSound("assets/vertical_slice/tactical_combat/audio/tac_victory.wav", 0.46f);
                return;
            }
            if (!HasAliveTeam(scene, (int)TacticalCombatTeam::Player))
            {
                level.RuntimePhase = TacticalCombatPhase::Defeat;
                level.RuntimeResultTimer = 0.0f;
                level.RuntimeMessage = "我方全灭。";
                return;
            }

            const TacticalCombatPhase returnPhase = level.RuntimeActionReturnPhase;
            level.RuntimeActionActorTag.clear();
            level.RuntimeActionTargetTag.clear();
            level.RuntimeActionSkillId.clear();

            if (returnPhase == TacticalCombatPhase::EnemyTurn)
            {
                level.RuntimePhase = TacticalCombatPhase::EnemyTurn;
                level.RuntimeEnemyCursor += 1;
                level.RuntimeEnemyStepTimer = 0.0f;
                return;
            }

            FinishSelectedPlayerAction(scene, level);
        }

        static void MoveUnitOneStepToward(Scene* scene,
            TacticalCombatLevelComponent& level,
            TacticalUnitComponent& unit,
            const TacticalUnitComponent& target)
        {
            int bestX = unit.GridX;
            int bestY = unit.GridY;
            int bestDistance = Distance(unit.GridX, unit.GridY, target.GridX, target.GridY);

            const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            for (const auto& dir : dirs)
            {
                const int nx = unit.GridX + dir[0];
                const int ny = unit.GridY + dir[1];
                if (!InBounds(level, nx, ny) || FindUnitAt(scene, nx, ny))
                    continue;

                const int candidate = Distance(nx, ny, target.GridX, target.GridY);
                if (candidate < bestDistance)
                {
                    bestDistance = candidate;
                    bestX = nx;
                    bestY = ny;
                }
            }

            if (bestX != unit.GridX || bestY != unit.GridY)
            {
                unit.GridX = bestX;
                unit.GridY = bestY;
                PlayTacticalSound("assets/vertical_slice/tactical_combat/audio/tac_move.wav", 0.38f);
            }
        }

        static void ProcessEnemyStep(Scene* scene, TacticalCombatLevelComponent& level)
        {
            const std::vector<Entity> units = CollectUnits(scene);
            std::vector<Entity> enemies;
            for (Entity unit : units)
            {
                const auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                if (tactical.Team == (int)TacticalCombatTeam::Enemy && tactical.RuntimeAlive)
                    enemies.push_back(unit);
            }

            if (level.RuntimeEnemyCursor >= (int)enemies.size())
            {
                level.RuntimeRound += 1;
                level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
                level.RuntimeSelectedUnitTag.clear();
                level.RuntimeSelectedSkillId.clear();
                level.RuntimeMessage = "第 " + std::to_string(level.RuntimeRound) + " 回合，我方行动。";
                for (Entity unit : units)
                {
                    auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                    tactical.RuntimeHasActed = false;
                    tactical.RuntimeMoved = false;
                    tactical.RuntimeGuarding = false;
                }
                return;
            }

            Entity enemy = enemies[level.RuntimeEnemyCursor];
            auto& enemyUnit = enemy.GetComponent<TacticalUnitComponent>();
            Entity target = FindNearestAliveEnemy(scene, enemyUnit);
            if (!target || !target.HasComponent<TacticalUnitComponent>())
            {
                level.RuntimeEnemyCursor += 1;
                return;
            }

            const auto& targetUnit = target.GetComponent<TacticalUnitComponent>();
            const std::string skillId = enemyUnit.Skill1Id.empty() ? enemyUnit.BasicSkillId : enemyUnit.Skill1Id;
            const TacticalSkillDefinition* skill = FindSkill(skillId);
            const int range = skill ? skill->Range : enemyUnit.AttackRange;

            if (Distance(enemyUnit.GridX, enemyUnit.GridY, targetUnit.GridX, targetUnit.GridY) > range)
            {
                MoveUnitOneStepToward(scene, level, enemyUnit, targetUnit);
                level.RuntimeMessage = enemyUnit.DisplayName + " 正在接近。";
                level.RuntimeEnemyCursor += 1;
                level.RuntimeEnemyStepTimer = 0.0f;
                return;
            }

            BeginAction(scene, level, enemy, skillId, target, TacticalCombatPhase::EnemyTurn);
        }

        static void HandleCellCommand(Scene* scene,
            TacticalCombatLevelComponent& level,
            int x,
            int y)
        {
            if (!InBounds(level, x, y))
                return;

            if (level.RuntimePhase == TacticalCombatPhase::PlayerTurn)
            {
                Entity occupant = FindUnitAt(scene, x, y);
                if (!occupant || !occupant.HasComponent<TacticalUnitComponent>())
                    return;

                auto& unit = occupant.GetComponent<TacticalUnitComponent>();
                if (unit.Team != (int)TacticalCombatTeam::Player || !unit.Controllable || unit.RuntimeHasActed)
                {
                    level.RuntimeMessage = "这个单位现在不能行动。";
                    return;
                }

                level.RuntimeSelectedUnitTag = occupant.GetName();
                level.RuntimeSelectedSkillId = unit.BasicSkillId;
                level.RuntimePhase = TacticalCombatPhase::AwaitCommand;
                level.RuntimeMessage = "选择移动格，或点击技能后选择目标。";
                PlayTacticalSound("assets/vertical_slice/tactical_combat/audio/tac_select.wav", 0.34f);
                return;
            }

            Entity selected = FindEntityByName(scene, level.RuntimeSelectedUnitTag);
            if (!selected || !selected.HasComponent<TacticalUnitComponent>())
                return;

            auto& selectedUnit = selected.GetComponent<TacticalUnitComponent>();
            Entity occupant = FindUnitAt(scene, x, y);

            if (level.RuntimePhase == TacticalCombatPhase::AwaitCommand)
            {
                if (occupant && occupant.HasComponent<TacticalUnitComponent>())
                {
                    auto& targetUnit = occupant.GetComponent<TacticalUnitComponent>();
                    const TacticalSkillDefinition* basic = FindSkill(selectedUnit.BasicSkillId);
                    if (basic && IsValidTarget(*basic, selectedUnit, targetUnit))
                    {
                        BeginAction(scene, level, selected, basic->Id, occupant, TacticalCombatPhase::PlayerTurn);
                        return;
                    }
                }

                if (CanMoveTo(scene, level, selectedUnit, x, y))
                {
                    selectedUnit.GridX = x;
                    selectedUnit.GridY = y;
                    selectedUnit.RuntimeMoved = true;
                    level.RuntimeMessage = selectedUnit.DisplayName + " 移动完成，可以继续攻击或待机。";
                    PlayTacticalSound("assets/vertical_slice/tactical_combat/audio/tac_move.wav", 0.38f);
                    return;
                }
            }

            if (level.RuntimePhase == TacticalCombatPhase::Targeting)
            {
                const TacticalSkillDefinition* skill = FindSkill(level.RuntimeSelectedSkillId);
                if (!skill || !occupant || !occupant.HasComponent<TacticalUnitComponent>())
                {
                    level.RuntimeMessage = "请选择有效目标。";
                    return;
                }

                auto& targetUnit = occupant.GetComponent<TacticalUnitComponent>();
                if (!IsValidTarget(*skill, selectedUnit, targetUnit))
                {
                    level.RuntimeMessage = "目标不在范围内。";
                    return;
                }

                BeginAction(scene, level, selected, skill->Id, occupant, TacticalCombatPhase::PlayerTurn);
            }
        }

        static void HandleSkillCommand(Scene* scene,
            TacticalCombatLevelComponent& level,
            const std::string& slot)
        {
            Entity selected = FindEntityByName(scene, level.RuntimeSelectedUnitTag);
            if (!selected || !selected.HasComponent<TacticalUnitComponent>())
                return;

            auto& unit = selected.GetComponent<TacticalUnitComponent>();
            if (slot == "cancel")
            {
                level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
                level.RuntimeSelectedUnitTag.clear();
                level.RuntimeSelectedSkillId.clear();
                level.RuntimeMessage = "重新选择我方单位。";
                return;
            }

            if (slot == "wait")
            {
                level.RuntimeSelectedSkillId = "guard_wait";
                BeginAction(scene, level, selected, "guard_wait", selected, TacticalCombatPhase::PlayerTurn);
                return;
            }

            const std::optional<std::string> skillId = ResolvePlayerSkillId(unit, slot);
            const TacticalSkillDefinition* skill = skillId ? FindSkill(*skillId) : nullptr;
            if (!skill)
            {
                level.RuntimeMessage = "这个技能槽还没有技能。";
                return;
            }

            level.RuntimeSelectedSkillId = skill->Id;
            if (skill->TargetRule == TacticalTargetRule::Self)
            {
                BeginAction(scene, level, selected, skill->Id, selected, TacticalCombatPhase::PlayerTurn);
                return;
            }

            level.RuntimePhase = TacticalCombatPhase::Targeting;
            level.RuntimeMessage = "选择 " + std::string(skill->DisplayName) + " 的目标。";
        }

        static void ProcessTacticalCommand(Scene* scene,
            TacticalCombatLevelComponent& level,
            const std::string& command)
        {
            const std::vector<std::string> parts = SplitCommand(command);
            if (parts.size() < 2 || parts[0] != "tactic")
                return;

            if (parts[1] == "cell" && parts.size() >= 4)
            {
                try
                {
                    HandleCellCommand(scene, level, std::stoi(parts[2]), std::stoi(parts[3]));
                }
                catch (...) {}
                return;
            }

            if (parts[1] == "skill" && parts.size() >= 3
                && (level.RuntimePhase == TacticalCombatPhase::AwaitCommand
                    || level.RuntimePhase == TacticalCombatPhase::Targeting))
            {
                HandleSkillCommand(scene, level, parts[2]);
            }
        }

        static void ResetLevel(Scene* scene, TacticalCombatLevelComponent& level)
        {
            level.RuntimeElapsed = 0.0f;
            level.RuntimeFadeAlpha = 1.0f;
            level.RuntimePhase = TacticalCombatPhase::Intro;
            level.RuntimeActionReturnPhase = TacticalCombatPhase::PlayerTurn;
            level.RuntimeRound = 1;
            level.RuntimeEnemyCursor = 0;
            level.RuntimeSelectedUnitTag.clear();
            level.RuntimeSelectedSkillId.clear();
            level.RuntimeActionActorTag.clear();
            level.RuntimeActionTargetTag.clear();
            level.RuntimeActionSkillId.clear();
            level.RuntimeMessage = "战旗假玩法启动。";
            level.RuntimeRequestedCommand.clear();
            level.RuntimeIntroTimer = 0.0f;
            level.RuntimeActionTimer = 0.0f;
            level.RuntimeEnemyStepTimer = 0.0f;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeInitialized = true;
            level.RuntimeActionApplied = false;
            level.RuntimeResultCommandIssued = false;

            for (Entity unit : CollectUnits(scene))
            {
                auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                tactical.Health = std::clamp(tactical.Health <= 0.0f ? tactical.MaxHealth : tactical.Health, 0.0f, tactical.MaxHealth);
                tactical.RuntimeAlive = tactical.Health > 0.0f;
                tactical.RuntimeHasActed = false;
                tactical.RuntimeMoved = false;
                tactical.RuntimeGuarding = false;
                tactical.RuntimeHitFlashTimer = 0.0f;
                tactical.RuntimeVisualClip.clear();
                tactical.RuntimeVisualTimer = 0.0f;
            }

            SetImageAlpha(scene, level.FadeEntityName, 1.0f);
            SetImageAlpha(scene, level.ActionEffectEntityName, 0.0f);
        }

        static void UpdateLevel(Scene* scene, TacticalCombatLevelComponent& level, float dt)
        {
            if (!level.RuntimeInitialized)
                ResetLevel(scene, level);

            level.RuntimeElapsed += dt;
            if (level.StartFadeDuration <= 0.0f)
                level.RuntimeFadeAlpha = 0.0f;
            else
                level.RuntimeFadeAlpha = std::max(0.0f, level.RuntimeFadeAlpha - dt / level.StartFadeDuration);
            SetImageAlpha(scene, level.FadeEntityName, level.RuntimeFadeAlpha);

            for (Entity unit : CollectUnits(scene))
            {
                auto& tactical = unit.GetComponent<TacticalUnitComponent>();
                if (tactical.RuntimeHitFlashTimer > 0.0f)
                    tactical.RuntimeHitFlashTimer = std::max(0.0f, tactical.RuntimeHitFlashTimer - dt);
                UpdateUnitVisual(scene, level, unit, tactical, dt);
            }

            switch (level.RuntimePhase)
            {
            case TacticalCombatPhase::Intro:
                level.RuntimeIntroTimer += dt;
                if (level.RuntimeIntroTimer >= level.IntroDuration)
                {
                    level.RuntimePhase = TacticalCombatPhase::PlayerTurn;
                    level.RuntimeMessage = "我方回合。点击角色，再移动或攻击。";
                }
                break;

            case TacticalCombatPhase::Acting:
                level.RuntimeActionTimer += dt;
                UpdateActionEffect(scene, level);
                if (!level.RuntimeActionApplied && level.RuntimeActionTimer >= level.ActionDuration * 0.45f)
                {
                    ApplyAction(scene, level);
                    level.RuntimeActionApplied = true;
                }
                if (level.RuntimeActionTimer >= level.ActionDuration)
                    EndAction(scene, level);
                break;

            case TacticalCombatPhase::EnemyTurn:
                level.RuntimeEnemyStepTimer += dt;
                if (level.RuntimeEnemyStepTimer >= level.EnemyStepDuration)
                {
                    level.RuntimeEnemyStepTimer = 0.0f;
                    ProcessEnemyStep(scene, level);
                }
                break;

            case TacticalCombatPhase::Victory:
                level.RuntimeResultTimer += dt;
                if (!level.RuntimeResultCommandIssued && level.RuntimeResultTimer >= level.VictoryReturnDelay)
                {
                    level.RuntimeRequestedCommand = level.VictorySceneCommand;
                    level.RuntimeResultCommandIssued = true;
                }
                break;

            case TacticalCombatPhase::Defeat:
                level.RuntimeResultTimer += dt;
                if (!level.RuntimeResultCommandIssued && level.RuntimeResultTimer >= level.DefeatReturnDelay)
                {
                    level.RuntimeRequestedCommand = level.DefeatSceneCommand;
                    level.RuntimeResultCommandIssued = true;
                }
                break;

            case TacticalCombatPhase::PlayerTurn:
            case TacticalCombatPhase::AwaitCommand:
            case TacticalCombatPhase::Targeting:
                break;
            }

            UpdateTiles(scene, level);
            UpdateStatusUI(scene, level);
            UpdateCommandUI(scene, level);
        }

    } // namespace

    void TacticalCombatSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<TacticalCombatLevelComponent>())
        {
            auto& level = registry.get<TacticalCombatLevelComponent>(entity);
            if (level.PlayOnStart)
                ResetLevel(scene, level);
        }
    }

    void TacticalCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        auto view = registry.view<TacticalCombatLevelComponent>();

        const std::vector<std::string> commands = CommandBus::DrainGameplayCommands("tactic:");
        for (const std::string& command : commands)
        {
            for (auto entity : view)
                ProcessTacticalCommand(scene, registry.get<TacticalCombatLevelComponent>(entity), command);
        }

        for (auto entity : view)
            UpdateLevel(scene, registry.get<TacticalCombatLevelComponent>(entity), ts);
    }

} // namespace Wheatear
