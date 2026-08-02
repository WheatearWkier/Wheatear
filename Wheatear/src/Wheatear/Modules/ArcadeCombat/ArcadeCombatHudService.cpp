#include "wtpch.h"
#include "ArcadeCombatHudService.h"

#include "Wheatear/Modules/Common/GameplayUIService.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <string>

namespace Wheatear::ArcadeCombatHudService {

    const char* WeaponName(ArcadeWeaponType weapon)
    {
        switch (weapon)
        {
        case ArcadeWeaponType::Gun:    return "手枪";
        case ArcadeWeaponType::Cannon: return "重炮";
        case ArcadeWeaponType::Katana: return "太刀";
        }
        return "手枪";
    }

    namespace {

        static std::string BuildMessage(const ArcadeCombatLevelComponent& level,
            const ArcadeCombatantComponent* player,
            const ArcadeBossComponent* boss)
        {
            if (level.RuntimePaused)
                return "暂停中。按 P 或 Esc 继续。";
            if (level.RuntimeDefeat)
                return "被击倒了，稍后返回重试。";
            if (level.RuntimeVictory)
                return "首领已击破，返回剧情。";
            if (level.RuntimeBossIntroStarted && !level.RuntimeBossIntroFinished)
                return "首领登场中，暂时无法行动。";
            if (!level.RuntimeBossIntroStarted)
                return "移动到发光点。WASD 移动，鼠标 / J 攻击，1/2/3 切换武器。";
            if (player && player->ControlsLocked)
                return "战斗演出中，暂时无法行动。";
            if (boss && boss->Active)
                return "战斗开始。利用掩体、切换武器，并保持移动。";
            return "准备。";
        }

    } // namespace

    void UpdateHUD(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss)
    {
        const ArcadeCombatantComponent* playerCombatant =
            player && player.HasComponent<ArcadeCombatantComponent>()
            ? &player.GetComponent<ArcadeCombatantComponent>()
            : nullptr;
        const ArcadeCombatantComponent* bossCombatant =
            boss && boss.HasComponent<ArcadeCombatantComponent>()
            ? &boss.GetComponent<ArcadeCombatantComponent>()
            : nullptr;
        const ArcadeBossComponent* bossComponent =
            boss && boss.HasComponent<ArcadeBossComponent>()
            ? &boss.GetComponent<ArcadeBossComponent>()
            : nullptr;

        if (playerCombatant)
        {
            GameplayUIService::SetHealth(scene,
                level.PlayerHealthBarEntityName,
                level.PlayerHealthTextEntityName,
                "生命",
                playerCombatant->Health,
                playerCombatant->MaxHealth);
        }

        if (bossCombatant)
        {
            GameplayUIService::SetHealth(scene,
                level.BossHealthBarEntityName,
                level.BossHealthTextEntityName,
                bossComponent && bossComponent->Active ? "首领" : "首领未明",
                bossCombatant->Health,
                bossCombatant->MaxHealth);
        }

        if (player && player.HasComponent<ArcadePlayerControllerComponent>())
        {
            const auto& controller = player.GetComponent<ArcadePlayerControllerComponent>();
            UIRuntimeTools::SetText(scene, level.WeaponTextEntityName,
                std::string("武器: ") + WeaponName(controller.CurrentWeapon) +
                "  [1 手枪] [2 重炮] [3 太刀]");
        }

        UIRuntimeTools::SetWidgetVisible(scene, level.PausePanelEntityName, level.RuntimePaused);
        UIRuntimeTools::SetText(scene, level.MessageTextEntityName, BuildMessage(level, playerCombatant, bossComponent));
    }

} // namespace Wheatear::ArcadeCombatHudService
