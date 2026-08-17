#include "wtpch.h"
#include "ArcadeCombatHudService.h"

#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Gameplay/Services/GameplayUIService.h"
#include "Wheatear/Gameplay/SystemBindingRegistry.h"
#include "Wheatear/Input/Input.h"
#include "Wheatear/Input/MouseButtonCodes.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>

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
                return "移动到发光点。左摇杆 / WASD 移动，攻击键 / J 攻击，武器框 / 1 2 3 切换。";
            if (player && player->ControlsLocked)
                return "战斗演出中，暂时无法行动。";
            if (boss && boss->Active)
                return "战斗开始。利用掩体、切换武器，并保持移动。";
            return "准备。";
        }


        // ---- on-screen touch controls -------------------------------------
        bool g_TouchDragging = false;
        glm::vec2 g_TouchMove = { 0.0f, 0.0f };
        bool g_TouchAttackHeld = false;
        int g_TouchWeaponPressed = -1;
        bool g_WeaponButtonLastHeld[3] = { false, false, false };

        static glm::vec2 TouchWindowSize()
        {
            Window& window = Application::Get().GetWindow();
            return {
                std::max(1.0f, static_cast<float>(window.GetWidth())),
                std::max(1.0f, static_cast<float>(window.GetHeight()))
            };
        }

        static bool PointInTouchWidget(Scene* scene,
            const std::string& name,
            const glm::vec2& mousePx,
            const glm::vec2& windowSize)
        {
            Entity widget = SceneQueries::FindEntityByName(scene, name);
            if (!widget || !widget.HasComponent<UIWidgetComponent>())
                return false;
            const auto& ui = widget.GetComponent<UIWidgetComponent>();
            const glm::vec2 minPx = ui.Position * windowSize;
            const glm::vec2 maxPx = (ui.Position + ui.Size) * windowSize;
            return mousePx.x >= minPx.x && mousePx.x <= maxPx.x
                && mousePx.y >= minPx.y && mousePx.y <= maxPx.y;
        }

        static void UpdateTouchControls(Scene* scene)
        {
            const glm::vec2 windowSize = TouchWindowSize();
            const glm::vec2 mousePx = { Input::GetMouseX(), Input::GetMouseY() };
            // IsMouseButtonPressed reports the held state (GLFW_PRESS).
            const bool leftDown = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);

            // Joystick: grab inside the base, steer (free direction), release.
            Entity base = SceneQueries::FindEntityByName(scene, SystemBindings::Arcade::JoystickBase);
            Entity thumb = SceneQueries::FindEntityByName(scene, SystemBindings::Arcade::JoystickThumb);
            if (base && base.HasComponent<UIWidgetComponent>())
            {
                const auto& baseWidget = base.GetComponent<UIWidgetComponent>();
                const glm::vec2 centerPx = (baseWidget.Position + baseWidget.Size * 0.5f) * windowSize;
                const glm::vec2 halfPx = baseWidget.Size * windowSize * 0.5f;

                if (!g_TouchDragging
                    && leftDown
                    && glm::abs(mousePx.x - centerPx.x) <= halfPx.x
                    && glm::abs(mousePx.y - centerPx.y) <= halfPx.y)
                {
                    g_TouchDragging = true;
                }
                if (g_TouchDragging && !leftDown)
                {
                    g_TouchDragging = false;
                    g_TouchMove = { 0.0f, 0.0f };
                }

                glm::vec2 stickPosition = baseWidget.Position;
                if (g_TouchDragging)
                {
                    glm::vec2 raw = (mousePx - centerPx) / halfPx;
                    const float rawLength = glm::length(raw);
                    if (rawLength > 1.0f)
                        raw /= rawLength;
                    g_TouchMove = raw;
                    stickPosition = baseWidget.Position + baseWidget.Size * 0.5f
                        + raw * (baseWidget.Size * 0.30f);
                }
                else
                {
                    stickPosition = baseWidget.Position + baseWidget.Size * 0.5f;
                }

                if (thumb && thumb.HasComponent<UIWidgetComponent>())
                {
                    auto& thumbWidget = thumb.GetComponent<UIWidgetComponent>();
                    thumbWidget.Position = stickPosition - thumbWidget.Size * 0.5f;
                }
            }

            // Attack button: held while pressed inside its area.
            g_TouchAttackHeld = leftDown
                && PointInTouchWidget(scene, SystemBindings::Arcade::AttackButton, mousePx, windowSize);

            // Weapon buttons: edge-triggered on press.
            g_TouchWeaponPressed = -1;
            for (int i = 0; i < 3; ++i)
            {
                const std::string name = SystemBindings::IndexedName(SystemBindings::Arcade::WeaponPrefix, i + 1);
                const bool held = leftDown
                    && PointInTouchWidget(scene, name, mousePx, windowSize);
                if (held && !g_WeaponButtonLastHeld[i])
                    g_TouchWeaponPressed = i;
                g_WeaponButtonLastHeld[i] = held;
            }
        }

    } // namespace

    void UpdateHUD(Scene* scene,
        ArcadeCombatLevelComponent& level,
        Entity player,
        Entity boss)
    {
        UpdateTouchControls(scene);

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


    glm::vec2 GetTouchMovement()
    {
        return g_TouchDragging ? g_TouchMove : glm::vec2(0.0f);
    }

    bool GetTouchAttackHeld()
    {
        return g_TouchAttackHeld;
    }

    int GetTouchWeaponPressed()
    {
        return g_TouchWeaponPressed;
    }

} // namespace Wheatear::ArcadeCombatHudService
