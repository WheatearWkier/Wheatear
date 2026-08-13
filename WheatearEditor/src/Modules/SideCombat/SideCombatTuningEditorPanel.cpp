#include "SideCombatTuningEditorPanel.h"

#include "Editor/EditorContentPickers.h"
#include "Editor/EditorFloatingWindow.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Editor/GameplayEditorShell.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/AssetPath.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <vector>

namespace Wheatear {

    namespace {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;

        using namespace EditorWidgets;

        static void DrawRawPreview(const std::string& text)
        {
            std::string preview = text;
            EditorWidgets::InputMultilineString("##SideCombatRawPreview",
                preview,
                ImVec2(-1.0f, -1.0f),
                std::max<size_t>(text.size() + 1, 4096),
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);
        }

    } // namespace

    namespace SideCombatEditorRequests {

        void RequestOpenTuning(const std::string& sourcePath)
        {
            s_PendingOpenPath = sourcePath;
            s_HasPendingOpen = true;
        }

        bool ConsumeOpenTuningRequest(std::string& sourcePath)
        {
            if (!s_HasPendingOpen)
                return false;

            sourcePath = s_PendingOpenPath;
            s_PendingOpenPath.clear();
            s_HasPendingOpen = false;
            return true;
        }

    } // namespace SideCombatEditorRequests

    SideCombatTuningEditorPanel::SideCombatTuningEditorPanel()
        : m_Root(std::make_unique<YAML::Node>(YAML::NodeType::Map))
    {
    }

    SideCombatTuningEditorPanel::~SideCombatTuningEditorPanel() = default;

    void SideCombatTuningEditorPanel::Open(const std::string& sourcePath)
    {
        m_Open = true;
        if (!sourcePath.empty() && sourcePath != m_SourcePath)
        {
            m_SourcePath = sourcePath;
            m_Loaded = false;
        }
    }

    void SideCombatTuningEditorPanel::OnImGuiRender()
    {
        std::string requestedPath;
        if (SideCombatEditorRequests::ConsumeOpenTuningRequest(requestedPath))
            Open(requestedPath);

        if (!m_Open)
            return;

        if (!m_Loaded)
            Load();

        EditorFloatingWindow::Begin("Side Combat Tuning Editor", &m_Open, 0, { 1180.0f, 760.0f });
        EditorWidgets::PanelHeader("Side Combat Tuning", "Structured YAML authoring for runtime movement, combo feel, attacks, skills, and progression profiles.");
        EditorFloatingWindow::DrawToggleButton("Side Combat Tuning Editor");
        EditorGameplayShell::DrawDocumentStatus({
            EditorGameplayShell::DocumentKind::Asset,
            m_Dirty,
            m_ParseValid,
            m_SourcePath,
            m_Status
        });
        DrawToolbar();

        if (!m_ParseValid)
        {
            ImGui::Separator();
            EditorWidgets::InlineStatus("YAML parse failed. Use the raw YAML editor to fix the file first.", EditorWidgets::StatusKind::Error);
            DrawRawPreview(m_RawPreview);
            EditorFloatingWindow::End();
            return;
        }

        if (ImGui::BeginTabBar("##SideCombatTuningTabs"))
        {
            if (ImGui::BeginTabItem("Feel"))
            {
                DrawFeelTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Rules"))
            {
                DrawRulesTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Attacks"))
            {
                DrawAttacksTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Animations"))
            {
                DrawAnimationsTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Skills"))
            {
                DrawSkillsTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Progression"))
            {
                DrawProgressionTab();
                ImGui::EndTabItem();
            }

            if (EditorGameplayShell::BeginRawPreviewTab("Advanced Raw"))
            {
                DrawRawPreviewTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        EditorFloatingWindow::End();
    }

    void SideCombatTuningEditorPanel::Load()
    {
        m_ResolvedPath = AssetPath::Resolve(AssetAliasRegistry::Resolve(m_SourcePath));
        m_Status.clear();
        m_RawPreview.clear();
        m_ParseValid = false;
        m_Dirty = false;

        std::string text;
        if (!ReadFileText(m_ResolvedPath, text))
        {
            *m_Root = YAML::Node(YAML::NodeType::Map);
            m_Status = "Load failed";
            m_Loaded = true;
            return;
        }

        m_RawPreview = text;

        try
        {
            *m_Root = YAML::Load(text);
            if (!m_Root->IsMap())
                *m_Root = YAML::Node(YAML::NodeType::Map);
            RefreshSelections();
            RefreshRawPreview();
            m_ParseValid = true;
            m_Status = "Loaded";
        }
        catch (const std::exception& e)
        {
            *m_Root = YAML::Node(YAML::NodeType::Map);
            m_Status = std::string("Parse failed: ") + e.what();
        }

        m_Loaded = true;
    }

    void SideCombatTuningEditorPanel::Save()
    {
        if (!m_ParseValid)
        {
            m_Status = "Save blocked: YAML is invalid";
            return;
        }

        RefreshRawPreview();
        const std::string output =
            "# Generated by Wheatear Side Combat Tuning Editor.\n"
            "# This file remains the runtime data source for SideCombat.\n\n" +
            m_RawPreview;

        if (!WriteFileText(m_ResolvedPath, output))
        {
            m_Status = "Save failed";
            return;
        }

        m_RawPreview = output;
        m_Dirty = false;
        m_Status = "Saved";
    }

    void SideCombatTuningEditorPanel::RefreshSelections()
    {
        YAML::Node root = *m_Root;
        const std::vector<std::string> attacks = MapKeys(root["attacks"]);
        if (std::find(attacks.begin(), attacks.end(), m_SelectedAttackId) == attacks.end())
            m_SelectedAttackId = attacks.empty() ? std::string{} : attacks.front();

        const std::vector<std::string> skills = MapKeys(root["skills"]);
        if (std::find(skills.begin(), skills.end(), m_SelectedSkillId) == skills.end())
            m_SelectedSkillId = skills.empty() ? std::string{} : skills.front();

        const std::vector<std::string> profiles = MapKeys(root["progression"]["profiles"]);
        if (std::find(profiles.begin(), profiles.end(), m_SelectedProfileId) == profiles.end())
            m_SelectedProfileId = profiles.empty() ? std::string{} : profiles.front();
    }

    void SideCombatTuningEditorPanel::RefreshRawPreview()
    {
        YAML::Emitter out;
        out.SetIndent(2);
        out << *m_Root;
        m_RawPreview = out.good() ? std::string(out.c_str()) : std::string{};
    }

    void SideCombatTuningEditorPanel::DrawToolbar()
    {
        EditorWidgets::SectionHeader("Source", "This YAML remains the runtime data source for SideCombat.");

        ImGui::PushItemWidth(-1.0f);
        if (InputString("Tuning YAML", m_SourcePath, 512))
            m_Loaded = false;
        ImGui::PopItemWidth();

        if (ImGui::Button("Load"))
            Load();

        ImGui::SameLine();
        bool reloadClicked = false;
        if (EditorWidgets::DirtySaveBar(m_Dirty, m_Status, "Save", "Reload", &reloadClicked))
            Save();
        if (reloadClicked)
            Load();

        if (!m_ResolvedPath.empty())
            ImGui::TextDisabled("%s", m_ResolvedPath.generic_string().c_str());
    }

    void SideCombatTuningEditorPanel::DrawFeelTab()
    {
        EditorWidgets::SectionHeader(EditorLocale::Text("Combat Feel", "战斗手感"), "Designer-facing controls mirrored from movement and air-combo YAML sections.");

        YAML::Node root = *m_Root;
        YAML::Node player = EnsureMap(root, "player");
        YAML::Node airCombo = EnsureMap(root, "airCombo");
        YAML::Node protection = EnsureMap(root, "protection");
        YAML::Node attacks = EnsureMap(root, "attacks");
        YAML::Node launcher = EnsureMap(attacks, "launcher");
        YAML::Node airBasic = EnsureMap(attacks, "air_basic");
        YAML::Node airChase = EnsureMap(attacks, "air_chase");
        YAML::Node dash = EnsureMap(attacks, "dash");
        YAML::Node breakLimit = EnsureMap(attacks, "break_limit");

        if (ImGui::CollapsingHeader(EditorLocale::Text("Movement / Jump", "移动 / 跳跃"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawFloat(player, "moveSpeed", "Move Speed", 0.02f, 0.0f, 30.0f)) m_Dirty = true;
            if (DrawInt(player, "maxJumps", "Max Jumps", 1, 3)) m_Dirty = true;
            if (DrawFloat(player, "jumpImpulse", "Jump Impulse", 0.05f, 0.0f, 40.0f)) m_Dirty = true;
            EditorWidgets::HelpTooltip("Jump height. Higher means the player reaches the airborne combo window more easily.");
            if (DrawFloat(player, "gravity", "Gravity", 0.05f, 0.0f, 80.0f)) m_Dirty = true;
            EditorWidgets::HelpTooltip("Falling speed. Lower means longer hang time.");
            if (DrawFloat(player, "airControl", "Air Control", 0.05f, 0.0f, 80.0f)) m_Dirty = true;
            if (DrawFloat(player, "jumpBufferTime", "Jump Buffer", 0.005f, 0.0f, 0.5f)) m_Dirty = true;
            if (DrawFloat(player, "coyoteTime", "Coyote Time", 0.005f, 0.0f, 0.5f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Dash", "冲刺"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawFloat(player, "dashCooldown", "Cooldown", 0.01f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(player, "dashManaCost", "Mana Cost", 0.5f, 0.0f, 100.0f)) m_Dirty = true;
            if (DrawFloat(player, "dashSpeed", "Speed", 0.05f, 0.0f, 40.0f)) m_Dirty = true;
            if (DrawFloat(player, "dashInvulnerableTime", "Invulnerable Time", 0.005f, 0.0f, 2.0f)) m_Dirty = true;
            if (DrawVec2(dash, "velocity", "Dash Velocity", 0.05f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Air Combo", "空中连段"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawInt(airCombo, "airActionLimit", "Air Action Limit", 0, 12)) m_Dirty = true;
            if (DrawFloat(airCombo, "airBasicCooldown", "Air Basic Cooldown", 0.01f, 0.01f, 3.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "airChaseCooldown", "Air Chase Cooldown", 0.01f, 0.01f, 3.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "groundThreatHeight", "Ground Threat Height", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "highAirSafetyHeight", "High Air Safety Height", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Launcher / Hang", "挑空 / 滞空"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawVec2(launcher, "launchVelocity", "S+J Launch Velocity", 0.05f)) m_Dirty = true;
            if (DrawFloat(launcher, "attackerAirImpulse", "S+J Player Lift", 0.05f, -20.0f, 30.0f)) m_Dirty = true;
            if (DrawFloat(launcher, "hitStun", "S+J Hit Stun", 0.01f, 0.0f, 5.0f)) m_Dirty = true;

            if (DrawVec2(airBasic, "launchVelocity", "Air J Target Lift", 0.05f)) m_Dirty = true;
            if (DrawFloat(airBasic, "attackerAirImpulse", "Air J Player Lift", 0.02f, -20.0f, 30.0f)) m_Dirty = true;
            if (DrawFloat(airBasic, "attackerAirFallStep", "Air J Player Fall Step", 0.005f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawFloat(airBasic, "targetAirFallStep", "Air J Target Fall Step", 0.005f, 0.0f, 3.0f)) m_Dirty = true;

            if (DrawVec2(airChase, "launchVelocity", "Air S+J Target Lift", 0.05f)) m_Dirty = true;
            if (DrawFloat(airChase, "attackerAirImpulse", "Air S+J Player Lift", 0.02f, -20.0f, 30.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Break Limit", "断限"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawBool(airCombo, "breakLimitEnabled", "Enable Break Limit")) m_Dirty = true;
            if (DrawInt(airCombo, "breakLimitMinCombo", "Min Combo", 0, 999)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitCooldown", "Cooldown", 0.02f, 0.0f, 30.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitGaugeCost", "Gauge Cost", 0.05f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(protection, "bossProtectionBreakLimitThreshold", EditorLocale::Text("Break Limit Threshold", "保护阈值"), 0.05f, 0.0f, 100.0f)) m_Dirty = true;
            if (DrawVec2(breakLimit, "launchVelocity", EditorLocale::Text("Hit Displacement", "命中位移"), 0.05f)) m_Dirty = true;
            if (DrawFloat(breakLimit, "hitStun", EditorLocale::Text("Break Hit Stun", "破防硬直"), 0.01f, 0.0f, 3.0f)) m_Dirty = true;
        }
    }

    void SideCombatTuningEditorPanel::DrawRulesTab()
    {
        EditorWidgets::SectionHeader(EditorLocale::Text("Runtime Rules", "运行时规则"), "Damage, feedback, boss protection, enemy pacing, pickup, and stage visual parameters.");

        YAML::Node root = *m_Root;
        YAML::Node player = EnsureMap(root, "player");
        YAML::Node combat = EnsureMap(root, "combat");
        YAML::Node feedback = EnsureMap(root, "feedback");
        YAML::Node airCombo = EnsureMap(root, "airCombo");
        YAML::Node protection = EnsureMap(root, "protection");
        YAML::Node enemy = EnsureMap(root, "enemy");
        YAML::Node bearBoss = EnsureMap(enemy, "bearBoss");
        YAML::Node pickup = EnsureMap(root, "pickup");
        YAML::Node movement = EnsureMap(root, "movement");
        YAML::Node visuals = EnsureMap(root, "visuals");

        if (ImGui::CollapsingHeader(EditorLocale::Text("Player", "玩家"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawFloat(player, "groundAcceleration", "Ground Acceleration", 0.05f, 0.0f, 120.0f)) m_Dirty = true;
            if (DrawFloat(player, "groundFriction", "Ground Friction", 0.05f, 0.0f, 120.0f)) m_Dirty = true;
            if (DrawFloat(player, "laneSpeedScale", "Lane Speed Scale", 0.01f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawFloat(player, "laneAcceleration", "Lane Acceleration", 0.05f, 0.0f, 120.0f)) m_Dirty = true;
            if (DrawFloat(player, "basicCooldown", "Ground Basic Cooldown", 0.01f, 0.01f, 3.0f)) m_Dirty = true;
            if (DrawFloat(player, "basicFinisherExtraCooldown", "Finisher Extra Cooldown", 0.01f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawFloat(player, "launcherCooldown", "Launcher Cooldown", 0.01f, 0.01f, 5.0f)) m_Dirty = true;
            if (DrawFloat(player, "magicBoltCooldown", "Magic Cooldown", 0.01f, 0.01f, 10.0f)) m_Dirty = true;
            if (DrawFloat(player, "allySupportCooldown", "Support Cooldown", 0.05f, 0.1f, 30.0f)) m_Dirty = true;
            if (DrawFloat(player, "basicChainWindow", "Basic Chain Window", 0.01f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawFloat(player, "launcherChainWindow", "Launcher Chain Window", 0.01f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawFloat(player, "magicChainWindow", "Magic Chain Window", 0.01f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawFloat(player, "supportChainWindow", "Support Chain Window", 0.01f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawFloat(player, "healItemCooldown", "Heal Item Cooldown", 0.1f, 0.0f, 30.0f)) m_Dirty = true;
            if (DrawFloat(player, "manaItemCooldown", "Mana Item Cooldown", 0.1f, 0.0f, 30.0f)) m_Dirty = true;
            if (DrawFloat(player, "attackBuffItemCooldown", "Attack Buff Item Cooldown", 0.1f, 0.0f, 60.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Damage / Combo", "伤害 / 连段")))
        {
            if (DrawFloat(combat, "comboDropDelay", "Combo Drop Delay", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(combat, "hitInvulnerableTime", "Hit Invulnerable Time", 0.005f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(combat, "defenseBase", "Defense Base", 0.5f, 1.0f, 9999.0f)) m_Dirty = true;
            if (DrawFloat(combat, "minDamage", "Min Damage", 0.1f, 0.0f, 999.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Feedback", "反馈")))
        {
            if (DrawFloat(feedback, "hitPauseTimeScale", "Hit Pause Time Scale", 0.01f, 0.01f, 1.0f)) m_Dirty = true;
            if (DrawString(feedback, "jumpSound", "Jump Sound")) m_Dirty = true;
            if (DrawString(feedback, "landSound", "Land Sound")) m_Dirty = true;
            if (DrawFloat(feedback, "jumpSoundVolume", "Jump Sound Volume", 0.01f, 0.0f, 2.0f)) m_Dirty = true;
            if (DrawFloat(feedback, "landSoundVolume", "Land Sound Volume", 0.01f, 0.0f, 2.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader("Air Combo / Protection"))
        {
            if (DrawInt(airCombo, "airActionLimitAfterBreak", "Air Actions After Break", 0, 12)) m_Dirty = true;
            if (DrawBool(airCombo, "breakLimitDebugKeyEnabled", "Break Limit Debug Key")) m_Dirty = true;
            if (DrawBool(airCombo, "showBreakLimitHint", "Show Break Limit Hint")) m_Dirty = true;
            if (DrawFloat(airCombo, "magicSwordGaugeMax", "Magic Sword Gauge Max", 0.05f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "gaugeGainGroundHit", "Gauge Gain Ground Hit", 0.01f, 0.0f, 5.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "gaugeGainAirHit", "Gauge Gain Air Hit", 0.01f, 0.0f, 5.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitMaxHeight", "Break Limit Max Height", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitFallingVelocity", "Break Limit Falling Velocity", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitHangImpulse", "Break Limit Hang Impulse", 0.02f, 0.0f, 20.0f)) m_Dirty = true;

            ImGui::Separator();
            if (DrawFloat(protection, "bossProtectionMax", "Boss Protection Max", 0.5f, 0.0f, 999.0f)) m_Dirty = true;
            if (DrawFloat(protection, "bossProtectionDecayPerSecond", "Protection Decay / Sec", 0.1f, 0.0f, 999.0f)) m_Dirty = true;
            if (DrawFloat(protection, "bossProtectionLimitTime", "Protection Limit Time", 0.01f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(protection, "bossProtectionForceFallVelocity", "Force Fall Velocity", 0.05f, -80.0f, 80.0f)) m_Dirty = true;
            if (DrawFloat(protection, "bossProtectionBreakLimitThreshold", "Break Limit Threshold", 0.5f, 0.0f, 999.0f)) m_Dirty = true;
            if (DrawFloat(protection, "breakLimitProtectionReduce", "Protection Reduce", 0.5f, 0.0f, 999.0f)) m_Dirty = true;
            if (DrawFloat(protection, "groundResetDelay", "Ground Reset Delay", 0.01f, 0.0f, 3.0f)) m_Dirty = true;
            if (DrawBool(protection, "showBossProtectionHud", "Show Boss Protection HUD")) m_Dirty = true;
            if (DrawBool(protection, "showCombatStateHud", "Show Combat State HUD")) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Enemy / Boss", "敌人 / Boss")))
        {
            if (DrawFloat(enemy, "initialAttackDelay", "Initial Attack Delay", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(enemy, "attackRangePadding", "Attack Range Padding", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(enemy, "laneAttackPadding", "Lane Attack Padding", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(enemy, "bossPreferredRangeBonus", "Boss Preferred Range Bonus", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(enemy, "gruntMoveSpeedScale", "Grunt Move Speed Scale", 0.01f, 0.0f, 5.0f)) m_Dirty = true;
            if (DrawFloat(enemy, "bossMoveSpeedScale", "Boss Move Speed Scale", 0.01f, 0.0f, 5.0f)) m_Dirty = true;
            if (DrawFloat(enemy, "xApproachAcceleration", "X Approach Accel", 0.1f, 0.0f, 120.0f)) m_Dirty = true;
            if (DrawFloat(enemy, "xBrakeAcceleration", "X Brake Accel", 0.1f, 0.0f, 120.0f)) m_Dirty = true;

            ImGui::Separator();
            if (DrawFloat(bearBoss, "moveSpeed", "Bear Move Speed", 0.02f, 0.0f, 30.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "aggroRange", "Aggro Range", 0.05f, 0.0f, 80.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "attackRange", "Attack Range", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "preferredRange", "Preferred Range", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "attackInterval", "Attack Interval", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "laneTolerance", "Lane Tolerance", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "midHealthThreshold", "Mid Health Threshold", 0.01f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "lowHealthThreshold", "Low Health Threshold", 0.01f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "midAttackInterval", "Mid Attack Interval", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "lowAttackInterval", "Low Attack Interval", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "chargeDistance", "Charge Distance", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "shockwaveDistance", "Shockwave Distance", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(bearBoss, "chargeSpeed", "Charge Speed", 0.02f, 0.0f, 40.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader(EditorLocale::Text("Pickup / Stage Visuals", "拾取 / 场景视觉")))
        {
            if (DrawFloat(pickup, "pickupRadius", "Pickup Radius", 0.01f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(pickup, "attractRadius", "Attract Radius", 0.05f, 0.0f, 50.0f)) m_Dirty = true;
            if (DrawFloat(pickup, "attractSpeed", "Attract Speed", 0.05f, 0.0f, 80.0f)) m_Dirty = true;

            ImGui::Separator();
            if (DrawFloat(movement, "laneMinY", "Lane Min Y", 0.02f, -20.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(movement, "laneMaxY", "Lane Max Y", 0.02f, -20.0f, 20.0f)) m_Dirty = true;
            if (DrawFloat(movement, "sortScale", "Sort Scale", 0.001f, 0.0f, 1.0f, "%.4f")) m_Dirty = true;

            ImGui::Separator();
            if (DrawFloat(visuals, "shadowMinAlpha", "Shadow Min Alpha", 0.01f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(visuals, "shadowMaxAlpha", "Shadow Max Alpha", 0.01f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(visuals, "shadowAirFadeHeight", "Shadow Air Fade Height", 0.05f, 0.0f, 20.0f)) m_Dirty = true;
        }
    }

    void SideCombatTuningEditorPanel::DrawAttacksTab()
    {
        EditorWidgets::SectionHeader(EditorLocale::Text("Attacks", "攻击"), "Author hitbox, frame, launch, VFX, and SFX parameters for each attack id.");

        YAML::Node root = *m_Root;
        YAML::Node attacks = EnsureMap(root, "attacks");
        const std::vector<std::string> keys = MapKeys(attacks);
        if (!BeginSelector("Attack", keys, m_SelectedAttackId))
        {
            EditorWidgets::EmptyState("No attacks in YAML.", "Add attack entries to the side-combat tuning data before authoring skills.");
            return;
        }

        YAML::Node attack = EnsureMap(attacks, m_SelectedAttackId.c_str());
        ImGui::Separator();
        ImGui::TextDisabled(EditorLocale::Text("Hitbox / Motion", "命中框 / 运动"));
        if (DrawVec2(attack, "size", "Size", 0.02f)) m_Dirty = true;
        if (DrawVec2(attack, "offset", "Offset", 0.02f)) m_Dirty = true;
        if (DrawVec2(attack, "velocity", "Projectile Velocity", 0.05f)) m_Dirty = true;
        if (DrawVec2(attack, "launchVelocity", "Launch Velocity", 0.05f)) m_Dirty = true;
        if (DrawFloat(attack, "airHeight", "Hitbox Air Height", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
        if (DrawFloat(attack, "airRange", "Hitbox Air Range", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
        if (DrawFloat(attack, "movementScale", "Movement Scale", 0.01f, 0.0f, 3.0f)) m_Dirty = true;

        ImGui::Separator();
        ImGui::TextDisabled(EditorLocale::Text("Damage / Frame Data", "伤害 / 帧数据"));
        if (DrawFloat(attack, "damageScale", "Damage Scale", 0.01f, 0.0f, 20.0f)) m_Dirty = true;
        if (DrawFloat(attack, "damageFlat", "Damage Flat", 0.5f, 0.0f, 9999.0f)) m_Dirty = true;
        if (DrawFloat(attack, "lifetime", "Hitbox Lifetime", 0.01f, 0.0f, 10.0f)) m_Dirty = true;
        if (DrawFloat(attack, "startup", "Startup", 0.005f, 0.0f, 5.0f)) m_Dirty = true;
        if (DrawFloat(attack, "recovery", "Recovery", 0.005f, 0.0f, 5.0f)) m_Dirty = true;
        if (DrawFloat(attack, "cancelWindowStart", "Cancel Start", 0.005f, 0.0f, 5.0f)) m_Dirty = true;
        if (DrawFloat(attack, "cancelWindowEnd", "Cancel End", 0.005f, 0.0f, 5.0f)) m_Dirty = true;
        if (DrawFloat(attack, "hitStun", "Hit Stun", 0.01f, 0.0f, 5.0f)) m_Dirty = true;
        if (DrawFloat(attack, "protectionGain", "Boss Protection Gain", 0.1f, 0.0f, 999.0f)) m_Dirty = true;
        if (DrawBool(attack, "destroyOnHit", "Destroy On Hit")) m_Dirty = true;

        ImGui::Separator();
        ImGui::TextDisabled(EditorLocale::Text("Air Combo Influence", "空中连段影响"));
        if (DrawFloat(attack, "attackerAirImpulse", "Attacker Air Impulse", 0.02f, -20.0f, 30.0f)) m_Dirty = true;
        if (DrawFloat(attack, "attackerAirFallStep", "Attacker Fall Step", 0.005f, 0.0f, 3.0f)) m_Dirty = true;
        if (DrawFloat(attack, "targetAirFallStep", "Target Fall Step", 0.005f, 0.0f, 3.0f)) m_Dirty = true;

        ImGui::Separator();
        ImGui::TextDisabled(EditorLocale::Text("VFX / SFX", "特效 / 音效"));
        if (DrawString(attack, "textureFramePattern", "Texture Frame Pattern")) m_Dirty = true;
        if (DrawInt(attack, "textureFrameCount", "Texture Frame Count", 1, 120)) m_Dirty = true;
        if (DrawFloat(attack, "textureFrameRate", "Texture Frame Rate", 0.5f, 1.0f, 120.0f)) m_Dirty = true;
        YAML::Node textureAtlas = EnsureMap(attack, "textureAtlas");
        if (DrawString(textureAtlas, "sheet", "Texture Sheet")) m_Dirty = true;
        if (DrawInt(textureAtlas, "cellWidth", "Sheet Cell Width", 0, 8192)) m_Dirty = true;
        if (DrawInt(textureAtlas, "cellHeight", "Sheet Cell Height", 0, 8192)) m_Dirty = true;
        if (DrawInt(textureAtlas, "columns", "Sheet Columns", 0, 256)) m_Dirty = true;
        if (DrawInt(textureAtlas, "startFrame", "Sheet Start Frame", 0, 4096)) m_Dirty = true;
        if (DrawString(attack, "swingSound", "Swing Sound")) m_Dirty = true;
        if (DrawString(attack, "hitSound", "Hit Sound")) m_Dirty = true;
        if (DrawFloat(attack, "soundVolume", "Sound Volume", 0.01f, 0.0f, 2.0f)) m_Dirty = true;
        if (DrawFloat(attack, "hitPause", "Hit Pause", 0.005f, 0.0f, 1.0f)) m_Dirty = true;
        if (DrawFloat(attack, "cameraShake", "Camera Shake", 0.001f, 0.0f, 1.0f, "%.4f")) m_Dirty = true;
        if (DrawFloat(attack, "cameraShakeDuration", "Shake Duration", 0.005f, 0.0f, 2.0f)) m_Dirty = true;
    }

    namespace {

        using namespace EditorWidgets;

        // Animations tab: visuals.<animMapsKey>[id] all share an identical schema
        //   { pattern, atlas{ sheet,cellWidth,cellHeight,columns,startFrame },
        //     frameCount, frameRate, loop, renderScale, renderOffset }
        // and are the largest hand-YAML section in side_combat_tuning.yaml.
        void DrawAnimEntry(YAML::Node entry, bool& dirty)
        {
            if (DrawString(entry, "pattern", "Pattern", 256)) dirty = true;

            YAML::Node atlas = EnsureMap(entry, "atlas");
            std::string sheet = EditorWidgets::ReadString(atlas, "sheet");
            if (EditorContentPickers::DrawAssetField(
                    "Atlas Sheet", sheet, EditorWidgets::AssetReferenceKind::Texture, 512))
            {
                atlas["sheet"] = sheet;
                dirty = true;
            }
            if (DrawInt(atlas, "cellWidth", "Cell Width", 0, 8192)) dirty = true;
            if (DrawInt(atlas, "cellHeight", "Cell Height", 0, 8192)) dirty = true;
            if (DrawInt(atlas, "columns", "Columns", 0, 256)) dirty = true;
            if (DrawInt(atlas, "startFrame", "Start Frame", 0, 4096)) dirty = true;

            if (DrawInt(entry, "frameCount", "Frame Count", 1, 256)) dirty = true;
            if (DrawFloat(entry, "frameRate", "Frame Rate", 0.5f, 1.0f, 120.0f)) dirty = true;
            if (DrawBool(entry, "loop", "Loop")) dirty = true;
            if (DrawVec2(entry, "renderScale", "Render Scale", 0.01f)) dirty = true;
            if (DrawVec2(entry, "renderOffset", "Render Offset", 0.005f)) dirty = true;
        }

        // Returns true while the caller should keep drawing the selected entry.
        bool DrawAnimSelectorAndOps(const char* groupLabel,
            YAML::Node animMaps,
            std::string& selectedId,
            std::array<char, 64>& newIdBuf,
            bool& dirty)
        {
            const std::vector<std::string> keys = MapKeys(animMaps);
            if (std::find(keys.begin(), keys.end(), selectedId) == keys.end())
                selectedId = keys.empty() ? std::string{} : keys.front();

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(groupLabel);
            ImGui::SameLine();
            ImGui::PushItemWidth(220.0f);
            const char* preview = selectedId.empty() ? "(none)" : selectedId.c_str();
            if (ImGui::BeginCombo("##AnimId", preview))
            {
                for (size_t i = 0; i < keys.size(); ++i)
                {
                    const std::string& key = keys[i];
                    const bool isSelected = selectedId == key;
                    const std::string itemLabel =
                        EditorWidgets::LabelWithId(key, "anim_selector:" + std::to_string(i));
                    if (ImGui::Selectable(itemLabel.c_str(), isSelected))
                        selectedId = key;
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Add"))
            {
                std::string newId = newIdBuf.data();
                // trim
                auto end = std::find(newId.begin(), newId.end(), '\0');
                if (end != newId.end()) newId.erase(end, newId.end());
                while (!newId.empty() && std::isspace(static_cast<unsigned char>(newId.front()))) newId.erase(newId.begin());
                while (!newId.empty() && std::isspace(static_cast<unsigned char>(newId.back()))) newId.pop_back();
                if (!newId.empty() && !animMaps[newId])
                {
                    EnsureMap(animMaps, newId.c_str());
                    selectedId = newId;
                    newIdBuf.fill('\0');
                    dirty = true;
                }
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(180.0f);
            ImGui::InputTextWithHint("##NewAnimId", "new animation id", newIdBuf.data(), newIdBuf.size());
            ImGui::PopItemWidth();

            if (!selectedId.empty() && !keys.empty())
            {
                ImGui::SameLine();
                if (ImGui::Button("Delete"))
                {
                    animMaps.remove(selectedId);
                    selectedId.clear();
                    dirty = true;
                }
            }

            return !selectedId.empty();
        }

    } // namespace

    void SideCombatTuningEditorPanel::DrawAnimationsTab()
    {
        EditorWidgets::SectionHeader(EditorLocale::Text("Animations", "动画"),
            "Atlas-backed animation clips for player / grunt / boss, plus stage shadow visuals. "
            "This is the largest section of the tuning YAML - author it here instead of by hand.");

        YAML::Node root = *m_Root;
        YAML::Node visuals = EnsureMap(root, "visuals");

        if (ImGui::CollapsingHeader(EditorLocale::Text("Stage Shadow", "场景阴影"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            // shadowColor is a 4-float sequence [r,g,b,a]; EditorWidgets has no DrawVec4,
            // so read/write it manually as two DragFloat2 pairs to stay in the helper idiom.
            float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            const YAML::Node shadowColor = visuals["shadowColor"];
            if (shadowColor && shadowColor.IsSequence() && shadowColor.size() >= 4)
            {
                for (int i = 0; i < 4; ++i)
                    color[i] = shadowColor[i].as<float>(color[i]);
            }
            const std::string shadowColorLabel = EditorWidgets::LabelWithId("Shadow Color", "shadowColor");
            bool changed = false;
            ImGui::PushID("shadowColorRGB");
            if (ImGui::DragFloat2("RGB", color, 0.01f, 0.0f, 1.0f, "%.3f")) changed = true;
            ImGui::PopID();
            ImGui::PushID("shadowColorA");
            if (ImGui::DragFloat("Alpha", &color[3], 0.01f, 0.0f, 1.0f, "%.3f")) changed = true;
            ImGui::PopID();
            (void)shadowColorLabel;
            if (changed)
            {
                YAML::Node seq(YAML::NodeType::Sequence);
                for (int i = 0; i < 4; ++i) seq.push_back(color[i]);
                visuals["shadowColor"] = seq;
                m_Dirty = true;
            }
            if (DrawVec2(visuals, "shadowOffset", "Shadow Offset", 0.01f)) m_Dirty = true;
            if (DrawFloat(visuals, "shadowMinAlpha", "Shadow Min Alpha", 0.01f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(visuals, "shadowMaxAlpha", "Shadow Max Alpha", 0.01f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(visuals, "shadowAirFadeHeight", "Shadow Air Fade Height", 0.05f, 0.0f, 20.0f)) m_Dirty = true;
        }

        auto drawGroup = [&](const char* groupLabel, const char* mapKey, std::string& selectedId)
        {
            if (!ImGui::CollapsingHeader(groupLabel, ImGuiTreeNodeFlags_DefaultOpen))
                return;
            YAML::Node animMaps = EnsureMap(visuals, mapKey);
            if (!DrawAnimSelectorAndOps(groupLabel, animMaps, selectedId, m_NewAnimId, m_Dirty))
            {
                ImGui::Separator();
                EditorWidgets::EmptyState("No animation selected.",
                    "Add an id above or pick an existing clip from the dropdown.");
                return;
            }
            ImGui::Separator();
            YAML::Node entry = EnsureMap(animMaps, selectedId.c_str());
            DrawAnimEntry(entry, m_Dirty);
        };

        drawGroup("Player Animations", "playerAnimations", m_SelectedPlayerAnimId);
        drawGroup("Grunt Animations", "gruntAnimations", m_SelectedGruntAnimId);
        drawGroup("Boss Animations", "bossAnimations", m_SelectedBossAnimId);
    }

    void SideCombatTuningEditorPanel::DrawSkillsTab()
    {
        EditorWidgets::SectionHeader(EditorLocale::Text("Skills", "技能"), "Skill ids bind display data, inputs, unlocks, and attack id chains.");

        YAML::Node root = *m_Root;
        YAML::Node skills = EnsureMap(root, "skills");
        const std::vector<std::string> keys = MapKeys(skills);
        if (!BeginSelector("Skill", keys, m_SelectedSkillId))
        {
            EditorWidgets::EmptyState("No skills in YAML.", "Add skill entries to expose player moves through data.");
            return;
        }

        YAML::Node skill = EnsureMap(skills, m_SelectedSkillId.c_str());
        ImGui::Separator();
        if (DrawString(skill, "displayName", "Display Name", 256)) m_Dirty = true;
        if (DrawString(skill, "input", "Input", 128)) m_Dirty = true;
        if (DrawString(skill, "comboRole", "Combo Role", 512)) m_Dirty = true;
        if (DrawStringList(skill, "attackIds", "Attack Ids", 512)) m_Dirty = true;
        if (DrawInt(skill, "unlockChapter", "Unlock Chapter", 0, 99)) m_Dirty = true;
        if (DrawBool(skill, "coreMove", "Core Move")) m_Dirty = true;
    }

    void SideCombatTuningEditorPanel::DrawProgressionTab()
    {
        EditorWidgets::SectionHeader(EditorLocale::Text("Progression", "成长"), "Profiles decide which skills and HUD systems are visible at runtime.");

        YAML::Node root = *m_Root;
        YAML::Node progression = EnsureMap(root, "progression");
        if (DrawString(progression, "defaultProfile", "Default Profile", 256)) m_Dirty = true;

        YAML::Node profiles = EnsureMap(progression, "profiles");
        const std::vector<std::string> keys = MapKeys(profiles);
        if (!BeginSelector("Profile", keys, m_SelectedProfileId))
        {
            EditorWidgets::EmptyState("No profiles in YAML.", "Add progression profiles so packaged builds can switch loadouts without recompiling.");
            return;
        }

        YAML::Node profile = EnsureMap(profiles, m_SelectedProfileId.c_str());
        YAML::Node visibleSystems = EnsureMap(profile, "visibleSystems");
        ImGui::Separator();
        if (DrawString(profile, "displayName", "Display Name", 256)) m_Dirty = true;
        if (DrawInt(profile, "chapter", "Chapter", 0, 99)) m_Dirty = true;
        if (DrawStringList(profile, "unlockedSkills", "Unlocked Skills", 1024)) m_Dirty = true;
        if (DrawStringList(profile, "debugSkills", "Debug Skills", 1024)) m_Dirty = true;

        ImGui::Separator();
        ImGui::TextDisabled(EditorLocale::Text("Visible Systems", "可见系统"));
        if (DrawBool(visibleSystems, "bossProtectionHud", "Boss Protection HUD")) m_Dirty = true;
        if (DrawBool(visibleSystems, "combatStateHud", "Combat State HUD")) m_Dirty = true;
        if (DrawBool(visibleSystems, "breakLimitHint", "Break Limit Hint")) m_Dirty = true;
    }

    void SideCombatTuningEditorPanel::DrawRawPreviewTab()
    {
        EditorWidgets::SectionHeader("Raw Preview", "Generated YAML preview. Save writes this text back to disk.");
        RefreshRawPreview();
        DrawRawPreview(m_RawPreview);
    }

} // namespace Wheatear
