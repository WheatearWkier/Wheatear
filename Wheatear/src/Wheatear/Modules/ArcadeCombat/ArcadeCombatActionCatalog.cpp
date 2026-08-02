#include "wtpch.h"
#include "ArcadeCombatActionCatalog.h"

#include "ArcadeCombatSignalHandlers.h"
#include "Wheatear/Gameplay/Action/ActionDatabase.h"

namespace Wheatear::ArcadeCombatActionCatalog {

    namespace {

        WAO::EffectSpec DamageEffect(float damage)
        {
            WAO::EffectSpec effect;
            effect.Type = WAO::EffectType::Damage;
            effect.Value = damage;
            return effect;
        }

        WAO::ActionRecipe MakeRecipe(const char* id,
            const char* name,
            const char* description,
            const char* icon,
            const char* animation,
            const char* sound,
            float cooldown,
            float damage)
        {
            WAO::ActionRecipe recipe;
            recipe.Id = id;
            recipe.DisplayName = name;
            recipe.Description = description;
            recipe.IconPath = icon;
            recipe.AnimationId = animation;
            recipe.SoundPath = sound;
            recipe.Cooldown = cooldown;
            recipe.Duration = cooldown;
            recipe.Startup = 0.0f;
            recipe.Recovery = 0.0f;
            recipe.HitTime = 0.0f;
            recipe.MovementScale = 1.0f;
            recipe.Tags = { "Gameplay.Arcade", "Gameplay.Combat" };
            recipe.Effects.push_back(DamageEffect(damage));
            recipe.Signals = { ArcadeCombatSignalHandlers::SpawnProjectileSignal };
            return recipe;
        }

    } // namespace

    const char* WeaponActionId(ArcadeWeaponType weapon)
    {
        switch (weapon)
        {
        case ArcadeWeaponType::Gun: return "arcade.gun";
        case ArcadeWeaponType::Cannon: return "arcade.cannon";
        case ArcadeWeaponType::Katana: return "arcade.katana";
        default: return "arcade.gun";
        }
    }

    WAO::ActionRecipe BuildWeaponRecipe(ArcadeWeaponType weapon)
    {
        switch (weapon)
        {
        case ArcadeWeaponType::Gun:
            return MakeRecipe("arcade.gun", "Gun", "Fast basic ranged shot.",
                "assets/vertical_slice/arcade_combat/ui/icons/arcade_gun.png",
                "arcade_gun",
                "assets/vertical_slice/arcade_combat/audio/arcade_gun.wav",
                0.16f,
                8.0f);
        case ArcadeWeaponType::Cannon:
            return MakeRecipe("arcade.cannon", "Cannon", "Slow heavy shot with splash pressure.",
                "assets/vertical_slice/arcade_combat/ui/icons/arcade_cannon.png",
                "arcade_cannon",
                "assets/vertical_slice/arcade_combat/audio/arcade_cannon.wav",
                0.75f,
                24.0f);
        case ArcadeWeaponType::Katana:
            return MakeRecipe("arcade.katana", "Katana", "Short melee slash weapon.",
                "assets/vertical_slice/arcade_combat/ui/icons/arcade_katana.png",
                "arcade_katana",
                "assets/vertical_slice/arcade_combat/audio/arcade_katana.wav",
                0.38f,
                18.0f);
        default:
            return BuildWeaponRecipe(ArcadeWeaponType::Gun);
        }
    }

    WAO::ActionRecipe BuildBossShotRecipe()
    {
        WAO::ActionRecipe recipe = MakeRecipe("arcade.boss_bullet",
            "Boss Bullet",
            "Standard boss projectile pattern.",
            "assets/vertical_slice/arcade_combat/ui/icons/arcade_boss_bullet.png",
            "arcade_boss_bullet",
            "assets/vertical_slice/arcade_combat/audio/arcade_boss_bullet.wav",
            1.05f,
            12.0f);
        recipe.Signals = { ArcadeCombatSignalHandlers::SpawnProjectileSignal };
        return recipe;
    }

    float PrimaryDamage(const WAO::ActionRecipe& recipe, float fallback)
    {
        for (const auto& effect : recipe.Effects)
        {
            if (effect.Type == WAO::EffectType::Damage)
                return effect.Value;
        }
        return fallback;
    }

    void RegisterActionRecipes()
    {
        WAO::ActionDatabase::Register(BuildWeaponRecipe(ArcadeWeaponType::Gun));
        WAO::ActionDatabase::Register(BuildWeaponRecipe(ArcadeWeaponType::Cannon));
        WAO::ActionDatabase::Register(BuildWeaponRecipe(ArcadeWeaponType::Katana));
        WAO::ActionDatabase::Register(BuildBossShotRecipe());
    }

} // namespace Wheatear::ArcadeCombatActionCatalog
