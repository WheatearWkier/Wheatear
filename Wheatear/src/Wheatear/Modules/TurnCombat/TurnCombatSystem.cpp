#include "wtpch.h"
#include "TurnCombatSystem.h"

#include "TurnCombatComponents.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "Wheatear/Modules/Progression/GameProgress.h"
#include "Wheatear/Runtime/CommandBus.h"
#include "Wheatear/Renderer/Texture.h"
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
#include <unordered_map>
#include <vector>

namespace Wheatear {

    namespace {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetImageAlpha;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetImageTexture;
        using UIRuntimeTools::SetProgress;
        using UIRuntimeTools::SetText;
        using UIRuntimeTools::SetWidgetVisible;

        struct TurnSkillDefinition
        {
            const char* Id = "";
            const char* DisplayName = "";
            const char* Description = "";
            const char* IconPath = "";
            const char* SoundPath = "";
            const char* EffectPath = "";
            TurnTargetRule TargetRule = TurnTargetRule::EnemySingle;
            float ManaCost = 0.0f;
            float Power = 1.0f;
            float HealPower = 0.0f;
            float DefensePierce = 0.0f;
            bool Magic = false;
            bool Guard = false;
        };

        static const std::vector<TurnSkillDefinition>& SkillLibrary()
        {
            static const std::vector<TurnSkillDefinition> skills = {
                {
                    "slash",
                    "魔剑斩",
                    "快速的魔剑斩击。稳定造成单体伤害。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_attack.png",
                    "assets/vertical_slice/turn_combat/audio/turn_slash.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_slash.png",
                    TurnTargetRule::EnemySingle,
                    0.0f,
                    1.00f,
                    0.0f,
                    0.10f,
                    false,
                    false
                },
                {
                    "aether_edge",
                    "灵素剑锋",
                    "消耗魔力发动更重的剑与魔法合击。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_magic_sword.png",
                    "assets/vertical_slice/turn_combat/audio/turn_magic.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_magic_sword.png",
                    TurnTargetRule::EnemySingle,
                    8.0f,
                    1.45f,
                    0.0f,
                    0.22f,
                    true,
                    false
                },
                {
                    "white_vow",
                    "白誓治愈",
                    "治疗一名我方角色。用于预览后续白魔法支援定位。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_heal.png",
                    "assets/vertical_slice/turn_combat/audio/turn_heal.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_heal.png",
                    TurnTargetRule::AllySingle,
                    7.0f,
                    0.0f,
                    1.25f,
                    0.0f,
                    true,
                    false
                },
                {
                    "shield_oath",
                    "守护誓约",
                    "本回合进入防御姿态，降低受到的伤害。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_guard.png",
                    "assets/vertical_slice/turn_combat/audio/turn_guard.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_guard.png",
                    TurnTargetRule::Self,
                    4.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                    false,
                    true
                },
                {
                    "black_flare",
                    "黑焰爆发",
                    "以不稳定的黑魔法攻击全体敌人。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_dark.png",
                    "assets/vertical_slice/turn_combat/audio/turn_magic.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_dark.png",
                    TurnTargetRule::EnemyAll,
                    12.0f,
                    0.86f,
                    0.0f,
                    0.18f,
                    true,
                    false
                },
                {
                    "focus_wait",
                    "凝神",
                    "跳过行动并回复魔力。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_wait.png",
                    "assets/vertical_slice/turn_combat/audio/turn_guard.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_focus.png",
                    TurnTargetRule::Self,
                    0.0f,
                    0.0f,
                    0.0f,
                    0.0f,
                    false,
                    false
                },
                {
                    "claw",
                    "爪击",
                    "魔物近身攻击。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_enemy_claw.png",
                    "assets/vertical_slice/turn_combat/audio/turn_hit.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_claw.png",
                    TurnTargetRule::EnemySingle,
                    0.0f,
                    0.95f,
                    0.0f,
                    0.0f,
                    false,
                    false
                },
                {
                    "wild_pounce",
                    "猛扑",
                    "更强的魔物攻击，用来测试防御收益。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_enemy_claw.png",
                    "assets/vertical_slice/turn_combat/audio/turn_hit.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_claw.png",
                    TurnTargetRule::EnemySingle,
                    0.0f,
                    1.25f,
                    0.0f,
                    0.08f,
                    false,
                    false
                },
                {
                    "dark_orb",
                    "暗影魔弹",
                    "精英法师的弱化魔法弹，提示魔法敌人的差异。",
                    "assets/vertical_slice/turn_combat/ui/icons/cmd_enemy_orb.png",
                    "assets/vertical_slice/turn_combat/audio/turn_magic.wav",
                    "assets/vertical_slice/turn_combat/effects/vfx_turn_dark.png",
                    TurnTargetRule::EnemySingle,
                    0.0f,
                    1.18f,
                    0.0f,
                    0.20f,
                    true,
                    false
                }
            };
            return skills;
        }

        static const TurnSkillDefinition* FindSkill(const std::string& id)
        {
            const auto& skills = SkillLibrary();
            auto it = std::find_if(skills.begin(), skills.end(),
                [&](const TurnSkillDefinition& skill) { return skill.Id == id; });
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

        static std::string JoinTurnOrder(Scene* scene, const TurnCombatLevelComponent& level)
        {
            std::ostringstream stream;
            int count = 0;
            for (int i = 0; i < (int)level.RuntimeTurnQueue.size() && count < 7; ++i)
            {
                const int index = (level.RuntimeTurnIndex + i) % (int)level.RuntimeTurnQueue.size();
                Entity entity = FindEntityByName(scene, level.RuntimeTurnQueue[index]);
                if (!entity || !entity.HasComponent<TurnCombatantComponent>())
                    continue;

                const auto& combatant = entity.GetComponent<TurnCombatantComponent>();
                if (!combatant.RuntimeAlive)
                    continue;

                if (count > 0)
                    stream << "  >  ";
                stream << combatant.DisplayName;
                ++count;
            }
            return stream.str();
        }

        static std::vector<Entity> CollectCombatants(Scene* scene)
        {
            std::vector<Entity> entities;
            if (!scene)
                return entities;

            auto& registry = scene->GetRegistry();
            for (auto entity : registry.view<TurnCombatantComponent>())
                entities.emplace_back(entity, scene);

            std::sort(entities.begin(), entities.end(), [](Entity a, Entity b)
            {
                const auto& ac = a.GetComponent<TurnCombatantComponent>();
                const auto& bc = b.GetComponent<TurnCombatantComponent>();
                if (ac.Team != bc.Team)
                    return ac.Team < bc.Team;
                return ac.Slot < bc.Slot;
            });
            return entities;
        }

        static std::vector<Entity> CollectAliveCombatants(Scene* scene, int team)
        {
            std::vector<Entity> entities;
            for (Entity entity : CollectCombatants(scene))
            {
                auto& combatant = entity.GetComponent<TurnCombatantComponent>();
                if (combatant.Team == team && combatant.RuntimeAlive)
                    entities.push_back(entity);
            }
            return entities;
        }

        static bool HasAliveTeam(Scene* scene, int team)
        {
            return !CollectAliveCombatants(scene, team).empty();
        }

        static void CacheVisuals(Entity entity)
        {
            if (!entity || !entity.HasComponent<TurnCombatantComponent>())
                return;

            auto& combatant = entity.GetComponent<TurnCombatantComponent>();
            if (combatant.RuntimeVisualCached || !entity.HasComponent<TransformComponent>())
                return;

            const auto& transform = entity.GetComponent<TransformComponent>();
            combatant.RuntimeBaseTranslation = transform.Translation;
            combatant.RuntimeBaseScale = transform.Scale;
            combatant.RuntimeVisualCached = true;
        }

        static void RestoreVisual(Entity entity)
        {
            if (!entity || !entity.HasComponent<TurnCombatantComponent>())
                return;

            auto& combatant = entity.GetComponent<TurnCombatantComponent>();
            if (entity.HasComponent<TransformComponent>() && combatant.RuntimeVisualCached)
            {
                auto& transform = entity.GetComponent<TransformComponent>();
                transform.Translation = combatant.RuntimeBaseTranslation;
                transform.Scale = combatant.RuntimeBaseScale;
            }

            if (entity.HasComponent<SpriteRendererComponent>())
                entity.GetComponent<SpriteRendererComponent>().Color = { 1.0f, 1.0f, 1.0f, combatant.RuntimeAlive ? 1.0f : 0.34f };
        }

        static void PlayTurnSound(const std::string& path, float volume = 0.55f)
        {
            if (path.empty())
                return;

            const auto& settings = GameProgress::GetState().Settings;
            const float master = AudioEngine::PercentToGain(static_cast<float>(settings.MasterVolume));
            const float sfx = AudioEngine::PercentToGain(static_cast<float>(settings.SFXVolume));
            AudioEngine::PlaySound(path, volume * master * sfx);
        }

        static Ref<Texture2D> LoadTurnTexture(const std::string& texturePath)
        {
            if (texturePath.empty())
                return nullptr;

            static std::unordered_map<std::string, Ref<Texture2D>> textureCache;
            if (auto it = textureCache.find(texturePath); it != textureCache.end())
                return it->second;

            Ref<Texture2D> texture = Texture2D::Create(texturePath);
            if (!texture || !texture->IsLoaded())
                return nullptr;

            textureCache[texturePath] = texture;
            return texture;
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

        static std::string FormatAnimationFramePath(const std::string& pattern, int frameIndex)
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

        static const std::string& AnimationPatternForClip(
            const TurnCombatantComponent& combatant,
            const std::string& clip)
        {
            if (clip == "attack")
                return combatant.AttackFramePattern;
            if (clip == "hit")
                return combatant.HitFramePattern;
            if (clip == "down")
                return combatant.DownFramePattern;
            return combatant.IdleFramePattern;
        }

        static int AnimationFrameCountForClip(
            const TurnCombatantComponent& combatant,
            const std::string& clip)
        {
            if (clip == "attack")
                return combatant.AttackFrameCount;
            if (clip == "hit")
                return combatant.HitFrameCount;
            if (clip == "down")
                return combatant.DownFrameCount;
            return combatant.IdleFrameCount;
        }

        static bool AnimationClipLoops(const std::string& clip)
        {
            return clip == "idle";
        }

        static std::string SelectCombatantAnimationClip(
            Entity entity,
            const TurnCombatantComponent& combatant,
            const TurnCombatLevelComponent& level)
        {
            if (!combatant.RuntimeAlive)
                return "down";
            if (combatant.RuntimeHitFlashTimer > 0.0f && !combatant.HitFramePattern.empty())
                return "hit";
            if (level.RuntimePhase == TurnCombatPhase::Acting
                && entity
                && entity.GetName() == level.RuntimeActionActorTag
                && !combatant.AttackFramePattern.empty())
            {
                return "attack";
            }
            return "idle";
        }

        static void ApplyCombatantAnimationFrame(
            Entity entity,
            TurnCombatantComponent& combatant,
            const TurnCombatLevelComponent& level,
            float dt)
        {
            if (!entity || !entity.HasComponent<SpriteRendererComponent>())
                return;

            const std::string clip = SelectCombatantAnimationClip(entity, combatant, level);
            const std::string& pattern = AnimationPatternForClip(combatant, clip);
            const int frameCount = std::max(AnimationFrameCountForClip(combatant, clip), 1);
            if (pattern.empty())
                return;

            auto& animator = entity.HasComponent<SpriteAnimatorComponent>()
                ? entity.GetComponent<SpriteAnimatorComponent>()
                : entity.AddComponent<SpriteAnimatorComponent>();
            animator.DefaultClipName = animator.DefaultClipName.empty() ? "idle" : animator.DefaultClipName;
            animator.PlayOnStart = false;
            animator.FireEvents = true;

            const auto ensureClip = [&](const std::string& clipName, const std::string& clipPattern, int clipFrameCount)
            {
                if (clipPattern.empty() || animator.Clips.find(clipName) != animator.Clips.end())
                    return;

                const bool looping = AnimationClipLoops(clipName);
                auto animationClip = AnimationClip::Create(clipName, looping);
                const int safeFrameCount = std::max(clipFrameCount, 1);
                const float duration = clipName == "attack"
                    ? std::max(level.ActionDuration, 0.01f) / (float)safeFrameCount
                    : 1.0f / std::max(combatant.AnimationFrameRate, 1.0f);

                for (int frameIndex = 0; frameIndex < safeFrameCount; ++frameIndex)
                {
                    if (Ref<Texture2D> texture = LoadTurnTexture(FormatAnimationFramePath(clipPattern, frameIndex)))
                        animationClip->AddFrame({ texture, duration });
                }

                if (animationClip->GetFrameCount() > 0)
                    animator.AddClip(animationClip);
            };

            ensureClip("idle", combatant.IdleFramePattern, combatant.IdleFrameCount);
            ensureClip("attack", combatant.AttackFramePattern, combatant.AttackFrameCount);
            ensureClip("hit", combatant.HitFramePattern, combatant.HitFrameCount);
            ensureClip("down", combatant.DownFramePattern, combatant.DownFrameCount);

            if (combatant.RuntimeAnimationClip != clip)
            {
                combatant.RuntimeAnimationClip = clip;
                combatant.RuntimeAnimationTimer = 0.0f;
                if (animator.Clips.find(clip) != animator.Clips.end())
                {
                    animator.CurrentClipName.clear();
                    animator.Play(clip);
                }
            }
            else
            {
                combatant.RuntimeAnimationTimer += dt;
                if (animator.CurrentClipName != clip && animator.Clips.find(clip) != animator.Clips.end())
                {
                    animator.CurrentClipName.clear();
                    animator.Play(clip);
                }
            }

            if (animator.Clips.find(clip) != animator.Clips.end())
                return;

            int frameIndex = 0;
            if (clip == "attack" && level.RuntimePhase == TurnCombatPhase::Acting)
            {
                const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 0.999f);
                frameIndex = std::min(frameCount - 1, (int)(t * (float)frameCount));
            }
            else if (AnimationClipLoops(clip))
            {
                frameIndex = (int)(combatant.RuntimeAnimationTimer * std::max(combatant.AnimationFrameRate, 1.0f)) % frameCount;
            }
            else
            {
                frameIndex = std::min(frameCount - 1,
                    (int)(combatant.RuntimeAnimationTimer * std::max(combatant.AnimationFrameRate, 1.0f)));
            }

            auto& sprite = entity.GetComponent<SpriteRendererComponent>();
            if (Ref<Texture2D> texture = LoadTurnTexture(FormatAnimationFramePath(pattern, frameIndex)))
                sprite.Texture = texture;
        }

        static void BuildTurnQueue(Scene* scene, TurnCombatLevelComponent& level)
        {
            std::vector<Entity> entities = CollectCombatants(scene);
            std::sort(entities.begin(), entities.end(), [](Entity a, Entity b)
            {
                const auto& ac = a.GetComponent<TurnCombatantComponent>();
                const auto& bc = b.GetComponent<TurnCombatantComponent>();
                if (std::abs(ac.Speed - bc.Speed) > 0.001f)
                    return ac.Speed > bc.Speed;
                return ac.Slot < bc.Slot;
            });

            level.RuntimeTurnQueue.clear();
            for (Entity entity : entities)
            {
                const auto& combatant = entity.GetComponent<TurnCombatantComponent>();
                if (combatant.RuntimeAlive)
                    level.RuntimeTurnQueue.push_back(entity.GetName());
            }
            level.RuntimeTurnIndex = 0;
        }

        static bool IsValidTarget(const TurnSkillDefinition& skill,
            const TurnCombatantComponent& actor,
            const TurnCombatantComponent& target)
        {
            if (!target.RuntimeAlive)
                return false;

            switch (skill.TargetRule)
            {
            case TurnTargetRule::EnemySingle:
            case TurnTargetRule::EnemyAll:
                return target.Team != actor.Team && target.Team != (int)TurnCombatTeam::Neutral;
            case TurnTargetRule::AllySingle:
            case TurnTargetRule::AllyAll:
                return target.Team == actor.Team;
            case TurnTargetRule::Self:
                return true;
            }
            return false;
        }

        static std::vector<Entity> ResolveTargets(Scene* scene,
            const TurnSkillDefinition& skill,
            Entity actor,
            Entity explicitTarget)
        {
            std::vector<Entity> targets;
            if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                return targets;

            const auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
            if (skill.TargetRule == TurnTargetRule::Self)
            {
                targets.push_back(actor);
                return targets;
            }

            if (skill.TargetRule == TurnTargetRule::EnemyAll || skill.TargetRule == TurnTargetRule::AllyAll)
            {
                for (Entity candidate : CollectCombatants(scene))
                {
                    if (!candidate.HasComponent<TurnCombatantComponent>())
                        continue;
                    const auto& targetCombatant = candidate.GetComponent<TurnCombatantComponent>();
                    if (IsValidTarget(skill, actorCombatant, targetCombatant))
                        targets.push_back(candidate);
                }
                return targets;
            }

            if (explicitTarget && explicitTarget.HasComponent<TurnCombatantComponent>())
            {
                const auto& targetCombatant = explicitTarget.GetComponent<TurnCombatantComponent>();
                if (IsValidTarget(skill, actorCombatant, targetCombatant))
                    targets.push_back(explicitTarget);
            }

            return targets;
        }

        static std::optional<std::string> ResolvePlayerSkillId(
            const TurnCombatantComponent& actor,
            const std::string& payload)
        {
            if (payload == "basic" || payload == "slot0")
                return actor.BasicSkillId;
            if (payload == "slot1")
                return actor.Skill1Id;
            if (payload == "slot2")
                return actor.Skill2Id;
            if (payload == "slot3")
                return actor.Skill3Id;
            if (FindSkill(payload))
                return payload;
            return std::nullopt;
        }

        static std::string ChooseEnemySkill(Entity actor, int round)
        {
            if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                return "claw";

            const auto& combatant = actor.GetComponent<TurnCombatantComponent>();
            if (!combatant.Skill2Id.empty() && round % 3 == 0)
                return combatant.Skill2Id;
            if (!combatant.Skill1Id.empty() && round % 2 == 0)
                return combatant.Skill1Id;
            if (!combatant.BasicSkillId.empty())
                return combatant.BasicSkillId;
            return "claw";
        }

        static Entity ChooseTargetForAI(Scene* scene, Entity actor, const TurnSkillDefinition& skill)
        {
            if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                return {};

            const auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
            std::vector<Entity> candidates;
            for (Entity candidate : CollectCombatants(scene))
            {
                if (!candidate.HasComponent<TurnCombatantComponent>())
                    continue;
                const auto& targetCombatant = candidate.GetComponent<TurnCombatantComponent>();
                if (IsValidTarget(skill, actorCombatant, targetCombatant))
                    candidates.push_back(candidate);
            }

            if (candidates.empty())
                return {};

            std::sort(candidates.begin(), candidates.end(), [](Entity a, Entity b)
            {
                const auto& ac = a.GetComponent<TurnCombatantComponent>();
                const auto& bc = b.GetComponent<TurnCombatantComponent>();
                return (ac.Health / std::max(ac.MaxHealth, 1.0f)) < (bc.Health / std::max(bc.MaxHealth, 1.0f));
            });
            return candidates.front();
        }

        static Entity FindTurnTarget(Scene* scene, const std::string& token)
        {
            if (!scene || token.empty())
                return {};

            Entity target = FindEntityByName(scene, token);
            if (target && target.HasComponent<TurnCombatantComponent>())
                return target;

            for (Entity candidate : CollectCombatants(scene))
            {
                const auto& combatant = candidate.GetComponent<TurnCombatantComponent>();
                if (combatant.TargetButtonEntityName == token
                    || combatant.TargetButtonEntityName == token + "_Target"
                    || combatant.TargetMarkerEntityName == token
                    || combatant.StatusTextEntityName == token)
                {
                    return candidate;
                }

                const std::string targetSuffix = "_Target";
                if (combatant.TargetButtonEntityName.size() > targetSuffix.size()
                    && combatant.TargetButtonEntityName.rfind(targetSuffix)
                        == combatant.TargetButtonEntityName.size() - targetSuffix.size()
                    && combatant.TargetButtonEntityName.substr(
                        0,
                        combatant.TargetButtonEntityName.size() - targetSuffix.size()) == token)
                {
                    return candidate;
                }
            }

            return {};
        }

        static float CalculateDamage(const TurnSkillDefinition& skill,
            const TurnCombatantComponent& actor,
            const TurnCombatantComponent& target)
        {
            const float offense = skill.Magic ? actor.Magic : actor.Attack;
            const float defense = std::max(0.0f, target.Defense * (1.0f - skill.DefensePierce));
            float damage = offense * skill.Power + 6.0f - defense * 0.62f;
            if (target.RuntimeGuarding)
                damage *= 0.45f;
            return std::max(1.0f, std::round(damage));
        }

        static float CalculateHeal(const TurnSkillDefinition& skill,
            const TurnCombatantComponent& actor)
        {
            return std::max(1.0f, std::round(actor.Magic * skill.HealPower + 18.0f));
        }

        static void MarkHit(Entity entity)
        {
            if (!entity || !entity.HasComponent<TurnCombatantComponent>())
                return;
            entity.GetComponent<TurnCombatantComponent>().RuntimeHitFlashTimer = 0.28f;
        }

        static void ApplySkill(Scene* scene, TurnCombatLevelComponent& level)
        {
            Entity actor = FindEntityByName(scene, level.RuntimeActionActorTag);
            Entity explicitTarget = FindEntityByName(scene, level.RuntimeActionTargetTag);
            const TurnSkillDefinition* skill = FindSkill(level.RuntimeActionSkillId);
            if (!actor || !actor.HasComponent<TurnCombatantComponent>() || !skill)
                return;

            auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
            actorCombatant.Mana = std::max(0.0f, actorCombatant.Mana - skill->ManaCost);
            actorCombatant.RuntimeGuarding = false;

            if (skill->Id == std::string("focus_wait"))
            {
                actorCombatant.Mana = std::min(actorCombatant.MaxMana, actorCombatant.Mana + 10.0f);
                level.RuntimeMessage = actorCombatant.DisplayName + " 凝聚魔力。";
                PlayTurnSound(skill->SoundPath, 0.42f);
                return;
            }

            if (skill->Guard)
            {
                actorCombatant.RuntimeGuarding = true;
                actorCombatant.Mana = std::min(actorCombatant.MaxMana, actorCombatant.Mana + 3.0f);
                level.RuntimeMessage = actorCombatant.DisplayName + " 进入防御姿态。";
                PlayTurnSound(skill->SoundPath, 0.45f);
                return;
            }

            std::vector<Entity> targets = ResolveTargets(scene, *skill, actor, explicitTarget);
            if (targets.empty())
                return;

            float total = 0.0f;
            for (Entity target : targets)
            {
                auto& targetCombatant = target.GetComponent<TurnCombatantComponent>();
                if (skill->HealPower > 0.0f)
                {
                    const float heal = CalculateHeal(*skill, actorCombatant);
                    targetCombatant.Health = std::min(targetCombatant.MaxHealth, targetCombatant.Health + heal);
                    total += heal;
                    MarkHit(target);
                }
                else
                {
                    if (targetCombatant.Invulnerable)
                        continue;

                    const float damage = CalculateDamage(*skill, actorCombatant, targetCombatant);
                    targetCombatant.Health = std::max(0.0f, targetCombatant.Health - damage);
                    targetCombatant.RuntimeAlive = targetCombatant.Health > 0.0f;
                    total += damage;
                    MarkHit(target);
                }
            }

            std::ostringstream stream;
            stream << actorCombatant.DisplayName << " 使用 " << skill->DisplayName;
            if (skill->HealPower > 0.0f)
                stream << "，恢复 " << (int)total;
            else
                stream << "，造成 " << (int)total << " 伤害";
            level.RuntimeMessage = stream.str();
            PlayTurnSound(skill->SoundPath, 0.52f);
        }

        static void BeginAction(Scene* scene,
            TurnCombatLevelComponent& level,
            Entity actor,
            const std::string& skillId,
            Entity target)
        {
            const TurnSkillDefinition* skill = FindSkill(skillId);
            if (!actor || !actor.HasComponent<TurnCombatantComponent>() || !skill)
                return;

            auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
            level.RuntimePhase = TurnCombatPhase::Acting;
            level.RuntimeActionTimer = 0.0f;
            level.RuntimeActionApplied = false;
            level.RuntimeActionActorTag = actor.GetName();
            level.RuntimeActionTargetTag = target ? target.GetName() : "";
            level.RuntimeActionSkillId = skillId;
            level.RuntimeSelectedSkillId.clear();

            std::ostringstream stream;
            stream << actorCombatant.DisplayName << " 准备 " << skill->DisplayName;
            level.RuntimeMessage = stream.str();
            SetWidgetVisible(scene, level.CommandPanelEntityName, false);
        }

        static void BeginNextTurn(Scene* scene, TurnCombatLevelComponent& level)
        {
            if (!HasAliveTeam(scene, (int)TurnCombatTeam::Player))
            {
                level.RuntimePhase = TurnCombatPhase::Defeat;
                level.RuntimeResultTimer = 0.0f;
                level.RuntimeMessage = "队伍全灭。";
                return;
            }

            if (!HasAliveTeam(scene, (int)TurnCombatTeam::Enemy))
            {
                level.RuntimePhase = TurnCombatPhase::Victory;
                level.RuntimeResultTimer = 0.0f;
                level.RuntimeMessage = "胜利！";
                return;
            }

            if (level.RuntimeTurnQueue.empty() || level.RuntimeTurnIndex >= (int)level.RuntimeTurnQueue.size())
            {
                BuildTurnQueue(scene, level);
                ++level.RuntimeRound;
            }

            int safety = 0;
            while (safety++ < 64 && !level.RuntimeTurnQueue.empty())
            {
                if (level.RuntimeTurnIndex >= (int)level.RuntimeTurnQueue.size())
                {
                    BuildTurnQueue(scene, level);
                    ++level.RuntimeRound;
                }

                Entity actor = FindEntityByName(scene, level.RuntimeTurnQueue[level.RuntimeTurnIndex]);
                ++level.RuntimeTurnIndex;
                if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                    continue;

                auto& combatant = actor.GetComponent<TurnCombatantComponent>();
                if (!combatant.RuntimeAlive)
                    continue;

                level.RuntimeActiveActorTag = actor.GetName();
                if (combatant.Team == (int)TurnCombatTeam::Player && combatant.Controllable)
                {
                    level.RuntimePhase = TurnCombatPhase::AwaitCommand;
                    level.RuntimeSelectedSkillId.clear();
                    level.RuntimeMessage = combatant.DisplayName + " 的回合。";
                    return;
                }

                const std::string skillId = ChooseEnemySkill(actor, level.RuntimeRound);
                const TurnSkillDefinition* skill = FindSkill(skillId);
                Entity target = skill ? ChooseTargetForAI(scene, actor, *skill) : Entity{};
                BeginAction(scene, level, actor, skillId, target);
                return;
            }
        }

        static void ResetLevel(Scene* scene, TurnCombatLevelComponent& level)
        {
            level.RuntimeElapsed = 0.0f;
            level.RuntimeFadeAlpha = 1.0f;
            level.RuntimePhase = TurnCombatPhase::Intro;
            level.RuntimeRound = 1;
            level.RuntimeTurnIndex = 0;
            level.RuntimeTurnQueue.clear();
            level.RuntimeActiveActorTag.clear();
            level.RuntimeSelectedSkillId.clear();
            level.RuntimeActionActorTag.clear();
            level.RuntimeActionTargetTag.clear();
            level.RuntimeActionSkillId.clear();
            level.RuntimeMessage = "回合战斗开始。";
            level.RuntimeRequestedCommand.clear();
            level.RuntimeIntroTimer = 0.0f;
            level.RuntimeActionTimer = 0.0f;
            level.RuntimeResultTimer = 0.0f;
            level.RuntimeInitialized = true;
            level.RuntimeActionApplied = false;
            level.RuntimeResultCommandIssued = false;

            for (Entity entity : CollectCombatants(scene))
            {
                auto& combatant = entity.GetComponent<TurnCombatantComponent>();
                combatant.Health = std::clamp(combatant.Health <= 0.0f ? combatant.MaxHealth : combatant.Health, 0.0f, combatant.MaxHealth);
                combatant.Mana = std::clamp(combatant.Mana <= 0.0f ? combatant.MaxMana : combatant.Mana, 0.0f, combatant.MaxMana);
                combatant.RuntimeAlive = combatant.Health > 0.0f;
                combatant.RuntimeGuarding = false;
                combatant.RuntimeSelectedTarget = false;
                combatant.RuntimeHitFlashTimer = 0.0f;
                combatant.RuntimeAnimationClip.clear();
                combatant.RuntimeAnimationTimer = 0.0f;
                CacheVisuals(entity);
                RestoreVisual(entity);
            }

            SetImageAlpha(scene, level.FadeEntityName, 1.0f);
            SetImageAlpha(scene, level.ActionFlashEntityName, 0.0f);
            BuildTurnQueue(scene, level);
        }

        static void UpdateStatusUI(Scene* scene, TurnCombatLevelComponent& level)
        {
            Entity active = FindEntityByName(scene, level.RuntimeActiveActorTag);
            const TurnSkillDefinition* selectedSkill = FindSkill(level.RuntimeSelectedSkillId);
            const TurnCombatantComponent* activeCombatant = (active && active.HasComponent<TurnCombatantComponent>())
                ? &active.GetComponent<TurnCombatantComponent>()
                : nullptr;

            for (Entity entity : CollectCombatants(scene))
            {
                auto& combatant = entity.GetComponent<TurnCombatantComponent>();
                SetProgress(scene, combatant.HealthBarEntityName, combatant.Health, combatant.MaxHealth);
                SetProgress(scene, combatant.ManaBarEntityName, combatant.Mana, combatant.MaxMana);

                std::ostringstream status;
                status << combatant.DisplayName << "  生命 " << (int)combatant.Health << "/" << (int)combatant.MaxHealth;
                if (combatant.Team == (int)TurnCombatTeam::Player)
                    status << "  魔力 " << (int)combatant.Mana << "/" << (int)combatant.MaxMana;
                if (combatant.RuntimeGuarding)
                    status << "  防御";
                if (!combatant.RuntimeAlive)
                    status << "  倒下";
                SetText(scene, combatant.StatusTextEntityName, status.str());

                const bool targetVisible = level.RuntimePhase == TurnCombatPhase::AwaitTarget
                    && selectedSkill
                    && activeCombatant
                    && IsValidTarget(*selectedSkill, *activeCombatant, combatant);
                SetWidgetVisible(scene, combatant.TargetButtonEntityName, targetVisible);
                SetWidgetVisible(scene, combatant.TargetMarkerEntityName, targetVisible);
            }
        }

        static void UpdateCommandUI(Scene* scene, TurnCombatLevelComponent& level)
        {
            const bool commandVisible = level.RuntimePhase == TurnCombatPhase::AwaitCommand
                || level.RuntimePhase == TurnCombatPhase::AwaitTarget;
            SetWidgetVisible(scene, level.CommandPanelEntityName, commandVisible);
            SetWidgetVisible(scene, level.TargetHintTextEntityName, level.RuntimePhase == TurnCombatPhase::AwaitTarget);

            Entity actor = FindEntityByName(scene, level.RuntimeActiveActorTag);
            if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                return;

            const auto& combatant = actor.GetComponent<TurnCombatantComponent>();
            const std::string skillSlots[] = {
                combatant.BasicSkillId,
                combatant.Skill1Id,
                combatant.Skill2Id,
                combatant.Skill3Id,
                "focus_wait"
            };

            for (int i = 0; i < 5; ++i)
            {
                const TurnSkillDefinition* skill = FindSkill(skillSlots[i]);
                const std::string prefix = "TC_Command_" + std::to_string(i + 1);
                if (!skill)
                {
                    SetWidgetVisible(scene, prefix + "_Root", false);
                    continue;
                }

                SetWidgetVisible(scene, prefix + "_Root", commandVisible);
                SetText(scene, prefix + "_Text", skill->DisplayName);
                SetImageTexture(scene, prefix + "_Icon", skill->IconPath, true);

                Entity icon = FindEntityByName(scene, prefix + "_Icon");
                if (icon && icon.HasComponent<UIImageComponent>())
                {
                    const bool affordable = combatant.Mana >= skill->ManaCost;
                    icon.GetComponent<UIImageComponent>().Color = affordable
                        ? glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
                        : glm::vec4{ 0.35f, 0.38f, 0.42f, 0.82f };
                }
            }

            const TurnSkillDefinition* selected = level.RuntimeSelectedSkillId.empty()
                ? FindSkill(combatant.BasicSkillId)
                : FindSkill(level.RuntimeSelectedSkillId);
            if (selected)
            {
                std::ostringstream details;
                details << selected->DisplayName << "  消耗魔力 " << (int)selected->ManaCost << "\n"
                    << selected->Description;
                SetText(scene, level.SkillDetailTextEntityName, details.str());
            }
        }

        static void UpdateBattleUI(Scene* scene, TurnCombatLevelComponent& level)
        {
            SetText(scene, level.MessageTextEntityName, level.RuntimeMessage);

            Entity active = FindEntityByName(scene, level.RuntimeActiveActorTag);
            if (active && active.HasComponent<TurnCombatantComponent>())
            {
                const auto& combatant = active.GetComponent<TurnCombatantComponent>();
                SetText(scene, level.ActiveActorTextEntityName, "行动中：" + combatant.DisplayName);
            }
            else
            {
                SetText(scene, level.ActiveActorTextEntityName, "行动中：-");
            }

            SetText(scene, level.TurnOrderTextEntityName, JoinTurnOrder(scene, level));
            UpdateStatusUI(scene, level);
            UpdateCommandUI(scene, level);
        }

        static void UpdateVisuals(Scene* scene, TurnCombatLevelComponent& level, float dt)
        {
            for (Entity entity : CollectCombatants(scene))
            {
                auto& combatant = entity.GetComponent<TurnCombatantComponent>();
                CacheVisuals(entity);

                if (combatant.RuntimeHitFlashTimer > 0.0f)
                    combatant.RuntimeHitFlashTimer = std::max(0.0f, combatant.RuntimeHitFlashTimer - dt);

                if (entity.HasComponent<SpriteRendererComponent>())
                {
                    ApplyCombatantAnimationFrame(entity, combatant, level, dt);
                    auto& sprite = entity.GetComponent<SpriteRendererComponent>();
                    if (!combatant.RuntimeAlive)
                    {
                        sprite.Color = { 0.45f, 0.48f, 0.52f, 0.35f };
                    }
                    else if (combatant.RuntimeHitFlashTimer > 0.0f)
                    {
                        sprite.Color = combatant.Team == (int)TurnCombatTeam::Player
                            ? glm::vec4{ 0.75f, 1.0f, 0.82f, 1.0f }
                            : glm::vec4{ 1.0f, 0.58f, 0.48f, 1.0f };
                    }
                    else
                    {
                        sprite.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
                    }
                }

                if (entity.HasComponent<TransformComponent>() && combatant.RuntimeVisualCached)
                {
                    auto& transform = entity.GetComponent<TransformComponent>();
                    transform.Translation = combatant.RuntimeBaseTranslation;
                    transform.Scale = combatant.RuntimeBaseScale;

                    if (entity.GetName() == level.RuntimeActionActorTag && level.RuntimePhase == TurnCombatPhase::Acting)
                    {
                        const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 1.0f);
                        const float pulse = std::sin(t * 3.14159265f);
                        const float direction = combatant.Team == (int)TurnCombatTeam::Player ? 1.0f : -1.0f;
                        transform.Translation.x += direction * pulse * 0.42f;
                        transform.Translation.y += pulse * 0.10f;
                        transform.Scale = combatant.RuntimeBaseScale * (1.0f + pulse * 0.055f);
                    }
                }
            }

            if (level.RuntimePhase == TurnCombatPhase::Acting)
            {
                const float t = std::clamp(level.RuntimeActionTimer / std::max(level.ActionDuration, 0.01f), 0.0f, 1.0f);
                const float alpha = std::max(0.0f, std::sin(t * 3.14159265f) * 0.20f);
                SetImageAlpha(scene, level.ActionFlashEntityName, alpha);

                Entity effect = FindEntityByName(scene, level.ActionEffectEntityName);
                const TurnSkillDefinition* skill = FindSkill(level.RuntimeActionSkillId);
                if (effect && skill && effect.HasComponent<SpriteRendererComponent>() && effect.HasComponent<TransformComponent>())
                {
                    auto& sprite = effect.GetComponent<SpriteRendererComponent>();
                    if (Ref<Texture2D> texture = LoadTurnTexture(skill->EffectPath))
                        sprite.Texture = texture;

                    const float effectAlpha = std::max(0.0f, std::sin(t * 3.14159265f));
                    sprite.Color = { 1.0f, 1.0f, 1.0f, effectAlpha };

                    Entity target = FindEntityByName(scene, level.RuntimeActionTargetTag);
                    if (!target)
                        target = FindEntityByName(scene, level.RuntimeActionActorTag);

                    if (target && target.HasComponent<TransformComponent>())
                    {
                        auto& transform = effect.GetComponent<TransformComponent>();
                        transform.Translation = target.GetComponent<TransformComponent>().Translation;
                        transform.Translation.z = -0.035f;
                        const float scale = 1.0f + effectAlpha * 0.22f;
                        transform.Scale = { scale * 1.55f, scale * 1.15f, 1.0f };
                        transform.Rotation.z = (skill->HealPower > 0.0f || skill->Guard) ? t * 4.8f : -0.35f + t * 0.45f;
                    }
                }
            }
            else
            {
                SetImageAlpha(scene, level.ActionFlashEntityName, 0.0f);
                Entity effect = FindEntityByName(scene, level.ActionEffectEntityName);
                if (effect && effect.HasComponent<SpriteRendererComponent>())
                    effect.GetComponent<SpriteRendererComponent>().Color.a = 0.0f;
            }
        }

        static void ProcessTurnCommand(Scene* scene, TurnCombatLevelComponent& level, const std::string& command)
        {
            const std::vector<std::string> parts = SplitCommand(command);
            if (parts.size() < 2 || parts[0] != "turn")
                return;

            Entity actor = FindEntityByName(scene, level.RuntimeActiveActorTag);
            if (!actor || !actor.HasComponent<TurnCombatantComponent>())
                return;

            auto& actorCombatant = actor.GetComponent<TurnCombatantComponent>();
            if (actorCombatant.Team != (int)TurnCombatTeam::Player || !actorCombatant.Controllable)
                return;

            if (parts[1] == "cancel" && level.RuntimePhase == TurnCombatPhase::AwaitTarget)
            {
                level.RuntimePhase = TurnCombatPhase::AwaitCommand;
                level.RuntimeSelectedSkillId.clear();
                level.RuntimeMessage = actorCombatant.DisplayName + " 的回合。";
                return;
            }

            if (parts[1] == "wait" && level.RuntimePhase == TurnCombatPhase::AwaitCommand)
            {
                BeginAction(scene, level, actor, "focus_wait", actor);
                return;
            }

            if (parts[1] == "skill" && parts.size() >= 3 && level.RuntimePhase == TurnCombatPhase::AwaitCommand)
            {
                const std::optional<std::string> resolvedSkillId = ResolvePlayerSkillId(actorCombatant, parts[2]);
                const TurnSkillDefinition* skill = resolvedSkillId ? FindSkill(*resolvedSkillId) : nullptr;
                if (!skill)
                {
                    level.RuntimeMessage = "这个技能槽还没有装备技能。";
                    return;
                }

                if (actorCombatant.Mana < skill->ManaCost)
                {
                    level.RuntimeMessage = "魔力不足。";
                    return;
                }

                if (skill->TargetRule == TurnTargetRule::Self)
                {
                    BeginAction(scene, level, actor, skill->Id, actor);
                    return;
                }

                if (skill->TargetRule == TurnTargetRule::EnemyAll || skill->TargetRule == TurnTargetRule::AllyAll)
                {
                    BeginAction(scene, level, actor, skill->Id, {});
                    return;
                }

                level.RuntimePhase = TurnCombatPhase::AwaitTarget;
                level.RuntimeSelectedSkillId = skill->Id;
                level.RuntimeMessage = "请选择目标。";
                return;
            }

            if (parts[1] == "target" && parts.size() >= 3 && level.RuntimePhase == TurnCombatPhase::AwaitTarget)
            {
                const TurnSkillDefinition* skill = FindSkill(level.RuntimeSelectedSkillId);
                Entity target = FindTurnTarget(scene, parts[2]);
                if (!skill || !target || !target.HasComponent<TurnCombatantComponent>())
                    return;

                if (!IsValidTarget(*skill, actorCombatant, target.GetComponent<TurnCombatantComponent>()))
                {
                    level.RuntimeMessage = "不能选择这个目标。";
                    return;
                }

                BeginAction(scene, level, actor, skill->Id, target);
            }
        }

        static void UpdateLevel(Scene* scene, TurnCombatLevelComponent& level, float dt)
        {
            if (!level.RuntimeInitialized)
                ResetLevel(scene, level);

            level.RuntimeElapsed += dt;
            if (level.StartFadeDuration <= 0.0f)
            {
                level.RuntimeFadeAlpha = 0.0f;
            }
            else
            {
                level.RuntimeFadeAlpha = std::max(0.0f,
                    level.RuntimeFadeAlpha - dt / level.StartFadeDuration);
            }
            SetImageAlpha(scene, level.FadeEntityName, level.RuntimeFadeAlpha);

            switch (level.RuntimePhase)
            {
            case TurnCombatPhase::Intro:
                level.RuntimeIntroTimer += dt;
                if (level.RuntimeIntroTimer >= level.IntroDuration)
                    BeginNextTurn(scene, level);
                break;

            case TurnCombatPhase::Acting:
                level.RuntimeActionTimer += dt;
                if (!level.RuntimeActionApplied && level.RuntimeActionTimer >= level.ActionDuration * 0.42f)
                {
                    ApplySkill(scene, level);
                    level.RuntimeActionApplied = true;
                }
                if (level.RuntimeActionTimer >= level.ActionDuration)
                    BeginNextTurn(scene, level);
                break;

            case TurnCombatPhase::Victory:
                level.RuntimeResultTimer += dt;
                SetWidgetVisible(scene, level.CommandPanelEntityName, false);
                if (!level.RuntimeResultCommandIssued && level.RuntimeResultTimer >= level.VictoryReturnDelay)
                {
                    level.RuntimeRequestedCommand = level.VictorySceneCommand;
                    level.RuntimeResultCommandIssued = true;
                }
                break;

            case TurnCombatPhase::Defeat:
                level.RuntimeResultTimer += dt;
                SetWidgetVisible(scene, level.CommandPanelEntityName, false);
                if (!level.RuntimeResultCommandIssued && level.RuntimeResultTimer >= level.DefeatReturnDelay)
                {
                    level.RuntimeRequestedCommand = level.DefeatSceneCommand;
                    level.RuntimeResultCommandIssued = true;
                }
                break;

            case TurnCombatPhase::AwaitCommand:
            case TurnCombatPhase::AwaitTarget:
                break;
            }

            UpdateVisuals(scene, level, dt);
            UpdateBattleUI(scene, level);
        }

    } // namespace

    void TurnCombatSystem::OnRuntimeStart(Scene* scene)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<TurnCombatLevelComponent>())
        {
            auto& level = registry.get<TurnCombatLevelComponent>(entity);
            if (level.PlayOnStart)
                ResetLevel(scene, level);
        }
    }

    void TurnCombatSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        if (!scene)
            return;

        auto& registry = scene->GetRegistry();
        auto view = registry.view<TurnCombatLevelComponent>();

        const std::vector<std::string> commands = CommandBus::DrainGameplayCommands("turn:");
        for (const std::string& command : commands)
        {
            for (auto entity : view)
                ProcessTurnCommand(scene, registry.get<TurnCombatLevelComponent>(entity), command);
        }

        for (auto entity : view)
            UpdateLevel(scene, registry.get<TurnCombatLevelComponent>(entity), ts);
    }

} // namespace Wheatear
