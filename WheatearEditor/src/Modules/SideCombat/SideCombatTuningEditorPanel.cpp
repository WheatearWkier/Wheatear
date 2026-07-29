#include "SideCombatTuningEditorPanel.h"

#include "Wheatear/Core/AssetPath.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace Wheatear {

    namespace {

        static bool s_HasPendingOpen = false;
        static std::string s_PendingOpenPath;

        template<typename T>
        static T ReadScalar(const YAML::Node& node, const char* key, T fallback)
        {
            try
            {
                const YAML::Node value = node[key];
                return value ? value.as<T>(fallback) : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }

        static std::string ReadString(const YAML::Node& node, const char* key, const std::string& fallback = {})
        {
            return ReadScalar<std::string>(node, key, fallback);
        }

        static YAML::Node EnsureMap(YAML::Node node, const char* key)
        {
            if (!node[key] || !node[key].IsMap())
                node[key] = YAML::Node(YAML::NodeType::Map);
            return node[key];
        }

        static std::vector<std::string> MapKeys(const YAML::Node& node)
        {
            std::vector<std::string> keys;
            if (!node || !node.IsMap())
                return keys;

            for (auto it = node.begin(); it != node.end(); ++it)
            {
                if (it->first.IsScalar())
                    keys.push_back(it->first.as<std::string>());
            }
            std::sort(keys.begin(), keys.end());
            return keys;
        }

        static bool ReadFileText(const std::filesystem::path& path, std::string& text)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
                return false;

            text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            return true;
        }

        static bool WriteFileText(const std::filesystem::path& path, const std::string& text)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
                return false;
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            return output.good();
        }

        static bool InputString(const char* label, std::string& value, size_t capacity = 512)
        {
            std::vector<char> buffer(std::max<size_t>(capacity, value.size() + 32), 0);
            strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
            if (ImGui::InputText(label, buffer.data(), buffer.size()))
            {
                value = buffer.data();
                return true;
            }
            return false;
        }

        static void Help(const char* text)
        {
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", text);
        }

        static bool DrawFloat(YAML::Node map,
            const char* key,
            const char* label,
            float speed = 0.01f,
            float minValue = 0.0f,
            float maxValue = 0.0f,
            const char* format = "%.3f")
        {
            float value = ReadScalar<float>(map, key, 0.0f);
            if (ImGui::DragFloat(label, &value, speed, minValue, maxValue, format))
            {
                map[key] = value;
                return true;
            }
            return false;
        }

        static bool DrawInt(YAML::Node map,
            const char* key,
            const char* label,
            int minValue = 0,
            int maxValue = 999)
        {
            int value = ReadScalar<int>(map, key, 0);
            if (ImGui::DragInt(label, &value, 1.0f, minValue, maxValue))
            {
                map[key] = value;
                return true;
            }
            return false;
        }

        static bool DrawBool(YAML::Node map, const char* key, const char* label)
        {
            bool value = ReadScalar<bool>(map, key, false);
            if (ImGui::Checkbox(label, &value))
            {
                map[key] = value;
                return true;
            }
            return false;
        }

        static bool DrawString(YAML::Node map,
            const char* key,
            const char* label,
            size_t capacity = 512)
        {
            std::string value = ReadString(map, key);
            if (InputString(label, value, capacity))
            {
                map[key] = value;
                return true;
            }
            return false;
        }

        static bool DrawVec2(YAML::Node map,
            const char* key,
            const char* label,
            float speed = 0.01f)
        {
            float values[2] = { 0.0f, 0.0f };
            const YAML::Node source = map[key];
            if (source && source.IsSequence() && source.size() >= 2)
            {
                values[0] = source[0].as<float>(0.0f);
                values[1] = source[1].as<float>(0.0f);
            }

            if (ImGui::DragFloat2(label, values, speed))
            {
                YAML::Node sequence(YAML::NodeType::Sequence);
                sequence.push_back(values[0]);
                sequence.push_back(values[1]);
                map[key] = sequence;
                return true;
            }
            return false;
        }

        static std::vector<std::string> ReadStringList(const YAML::Node& node)
        {
            std::vector<std::string> values;
            if (!node || !node.IsSequence())
                return values;

            for (std::size_t i = 0; i < node.size(); ++i)
            {
                const YAML::Node value = node[i];
                if (value.IsScalar())
                    values.push_back(value.as<std::string>());
            }
            return values;
        }

        static std::string JoinList(const std::vector<std::string>& values)
        {
            std::ostringstream stream;
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    stream << ", ";
                stream << values[i];
            }
            return stream.str();
        }

        static std::vector<std::string> SplitList(const std::string& text)
        {
            std::vector<std::string> values;
            std::stringstream stream(text);
            std::string item;
            while (std::getline(stream, item, ','))
            {
                const char* whitespace = " \t\r\n";
                const size_t begin = item.find_first_not_of(whitespace);
                if (begin == std::string::npos)
                    continue;
                const size_t end = item.find_last_not_of(whitespace);
                values.push_back(item.substr(begin, end - begin + 1));
            }
            return values;
        }

        static bool DrawStringList(YAML::Node map,
            const char* key,
            const char* label,
            size_t capacity = 512)
        {
            std::string value = JoinList(ReadStringList(map[key]));
            if (InputString(label, value, capacity))
            {
                YAML::Node sequence(YAML::NodeType::Sequence);
                for (const std::string& item : SplitList(value))
                    sequence.push_back(item);
                map[key] = sequence;
                return true;
            }
            return false;
        }

        static bool BeginSelector(const char* label,
            const std::vector<std::string>& keys,
            std::string& selected)
        {
            if (selected.empty() && !keys.empty())
                selected = keys.front();

            if (ImGui::BeginCombo(label, selected.empty() ? "(none)" : selected.c_str()))
            {
                for (const std::string& key : keys)
                {
                    const bool isSelected = selected == key;
                    if (ImGui::Selectable(key.c_str(), isSelected))
                        selected = key;
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            return !selected.empty();
        }

        static void DrawRawPreview(const std::string& text)
        {
            std::vector<char> buffer(std::max<size_t>(text.size() + 1, 4096), 0);
            strncpy_s(buffer.data(), buffer.size(), text.c_str(), _TRUNCATE);
            ImGui::InputTextMultiline("##SideCombatRawPreview",
                buffer.data(),
                buffer.size(),
                ImVec2(-1.0f, -1.0f),
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

        ImGui::Begin("Side Combat Tuning Editor", &m_Open);
        DrawToolbar();

        if (!m_ParseValid)
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.34f, 0.25f, 1.0f), "YAML parse failed. Use the raw YAML editor to fix the file first.");
            DrawRawPreview(m_RawPreview);
            ImGui::End();
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

            if (ImGui::BeginTabItem("Raw Preview"))
            {
                DrawRawPreviewTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void SideCombatTuningEditorPanel::Load()
    {
        m_ResolvedPath = AssetPath::Resolve(m_SourcePath);
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
        ImGui::PushItemWidth(-260.0f);
        if (InputString("Tuning YAML", m_SourcePath, 512))
            m_Loaded = false;
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Load"))
            Load();
        ImGui::SameLine();
        if (ImGui::Button("Save"))
            Save();

        if (m_Dirty)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.25f, 1.0f), "Modified");
        }

        if (!m_Status.empty())
            ImGui::TextDisabled("%s  %s", m_Status.c_str(), m_ResolvedPath.generic_string().c_str());
    }

    void SideCombatTuningEditorPanel::DrawFeelTab()
    {
        YAML::Node root = *m_Root;
        YAML::Node player = EnsureMap(root, "player");
        YAML::Node airCombo = EnsureMap(root, "airCombo");
        YAML::Node attacks = EnsureMap(root, "attacks");
        YAML::Node launcher = EnsureMap(attacks, "launcher");
        YAML::Node airBasic = EnsureMap(attacks, "air_basic");
        YAML::Node airChase = EnsureMap(attacks, "air_chase");
        YAML::Node breakLimit = EnsureMap(attacks, "break_limit");

        ImGui::TextDisabled("Common feel controls. These are mirrored from the YAML sections below.");

        if (ImGui::CollapsingHeader("Movement / Jump", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawFloat(player, "moveSpeed", "Move Speed", 0.02f, 0.0f, 30.0f)) m_Dirty = true;
            if (DrawInt(player, "maxJumps", "Max Jumps", 1, 3)) m_Dirty = true;
            if (DrawFloat(player, "jumpImpulse", "Jump Impulse", 0.05f, 0.0f, 40.0f)) m_Dirty = true;
            Help("Jump height. Higher means the player reaches the airborne combo window more easily.");
            if (DrawFloat(player, "gravity", "Gravity", 0.05f, 0.0f, 80.0f)) m_Dirty = true;
            Help("Falling speed. Lower means longer hang time.");
            if (DrawFloat(player, "airControl", "Air Control", 0.05f, 0.0f, 80.0f)) m_Dirty = true;
            if (DrawFloat(player, "jumpBufferTime", "Jump Buffer", 0.005f, 0.0f, 0.5f)) m_Dirty = true;
            if (DrawFloat(player, "coyoteTime", "Coyote Time", 0.005f, 0.0f, 0.5f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader("Air Combo", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawInt(airCombo, "airActionLimit", "Air Action Limit", 0, 12)) m_Dirty = true;
            if (DrawFloat(airCombo, "airBasicCooldown", "Air Basic Cooldown", 0.01f, 0.01f, 3.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "airChaseCooldown", "Air Chase Cooldown", 0.01f, 0.01f, 3.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "groundThreatHeight", "Ground Threat Height", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "highAirSafetyHeight", "High Air Safety Height", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader("Launcher / Hang", ImGuiTreeNodeFlags_DefaultOpen))
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

        if (ImGui::CollapsingHeader("Break Limit", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (DrawBool(airCombo, "breakLimitEnabled", "Enable Break Limit")) m_Dirty = true;
            if (DrawInt(airCombo, "breakLimitMinCombo", "Min Combo", 0, 999)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitCooldown", "Cooldown", 0.02f, 0.0f, 30.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitGaugeCost", "Gauge Cost", 0.05f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(airCombo, "breakLimitHeightBoost", "Height Boost", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawVec2(breakLimit, "launchVelocity", "Break Limit Launch", 0.05f)) m_Dirty = true;
            if (DrawFloat(breakLimit, "attackerAirImpulse", "Break Limit Player Lift", 0.02f, -20.0f, 30.0f)) m_Dirty = true;
        }
    }

    void SideCombatTuningEditorPanel::DrawRulesTab()
    {
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

        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen))
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
        }

        if (ImGui::CollapsingHeader("Damage / Combo"))
        {
            if (DrawFloat(combat, "comboDropDelay", "Combo Drop Delay", 0.02f, 0.0f, 10.0f)) m_Dirty = true;
            if (DrawFloat(combat, "hitInvulnerableTime", "Hit Invulnerable Time", 0.005f, 0.0f, 1.0f)) m_Dirty = true;
            if (DrawFloat(combat, "defenseBase", "Defense Base", 0.5f, 1.0f, 9999.0f)) m_Dirty = true;
            if (DrawFloat(combat, "minDamage", "Min Damage", 0.1f, 0.0f, 999.0f)) m_Dirty = true;
        }

        if (ImGui::CollapsingHeader("Feedback"))
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

        if (ImGui::CollapsingHeader("Enemy / Boss"))
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

        if (ImGui::CollapsingHeader("Pickup / Stage Visuals"))
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
        YAML::Node root = *m_Root;
        YAML::Node attacks = EnsureMap(root, "attacks");
        const std::vector<std::string> keys = MapKeys(attacks);
        if (!BeginSelector("Attack", keys, m_SelectedAttackId))
        {
            ImGui::TextDisabled("No attacks in YAML.");
            return;
        }

        YAML::Node attack = EnsureMap(attacks, m_SelectedAttackId.c_str());
        ImGui::Separator();
        ImGui::TextDisabled("Hitbox / Motion");
        if (DrawVec2(attack, "size", "Size", 0.02f)) m_Dirty = true;
        if (DrawVec2(attack, "offset", "Offset", 0.02f)) m_Dirty = true;
        if (DrawVec2(attack, "velocity", "Projectile Velocity", 0.05f)) m_Dirty = true;
        if (DrawVec2(attack, "launchVelocity", "Launch Velocity", 0.05f)) m_Dirty = true;
        if (DrawFloat(attack, "airHeight", "Hitbox Air Height", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
        if (DrawFloat(attack, "airRange", "Hitbox Air Range", 0.02f, 0.0f, 20.0f)) m_Dirty = true;
        if (DrawFloat(attack, "movementScale", "Movement Scale", 0.01f, 0.0f, 3.0f)) m_Dirty = true;

        ImGui::Separator();
        ImGui::TextDisabled("Damage / Frame Data");
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
        ImGui::TextDisabled("Air Combo Influence");
        if (DrawFloat(attack, "attackerAirImpulse", "Attacker Air Impulse", 0.02f, -20.0f, 30.0f)) m_Dirty = true;
        if (DrawFloat(attack, "attackerAirFallStep", "Attacker Fall Step", 0.005f, 0.0f, 3.0f)) m_Dirty = true;
        if (DrawFloat(attack, "targetAirFallStep", "Target Fall Step", 0.005f, 0.0f, 3.0f)) m_Dirty = true;

        ImGui::Separator();
        ImGui::TextDisabled("VFX / SFX");
        if (DrawString(attack, "textureFramePattern", "Texture Frame Pattern")) m_Dirty = true;
        if (DrawInt(attack, "textureFrameCount", "Texture Frame Count", 1, 120)) m_Dirty = true;
        if (DrawFloat(attack, "textureFrameRate", "Texture Frame Rate", 0.5f, 1.0f, 120.0f)) m_Dirty = true;
        if (DrawString(attack, "swingSound", "Swing Sound")) m_Dirty = true;
        if (DrawString(attack, "hitSound", "Hit Sound")) m_Dirty = true;
        if (DrawFloat(attack, "soundVolume", "Sound Volume", 0.01f, 0.0f, 2.0f)) m_Dirty = true;
        if (DrawFloat(attack, "hitPause", "Hit Pause", 0.005f, 0.0f, 1.0f)) m_Dirty = true;
        if (DrawFloat(attack, "cameraShake", "Camera Shake", 0.001f, 0.0f, 1.0f, "%.4f")) m_Dirty = true;
        if (DrawFloat(attack, "cameraShakeDuration", "Shake Duration", 0.005f, 0.0f, 2.0f)) m_Dirty = true;
    }

    void SideCombatTuningEditorPanel::DrawSkillsTab()
    {
        YAML::Node root = *m_Root;
        YAML::Node skills = EnsureMap(root, "skills");
        const std::vector<std::string> keys = MapKeys(skills);
        if (!BeginSelector("Skill", keys, m_SelectedSkillId))
        {
            ImGui::TextDisabled("No skills in YAML.");
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
        YAML::Node root = *m_Root;
        YAML::Node progression = EnsureMap(root, "progression");
        if (DrawString(progression, "defaultProfile", "Default Profile", 256)) m_Dirty = true;

        YAML::Node profiles = EnsureMap(progression, "profiles");
        const std::vector<std::string> keys = MapKeys(profiles);
        if (!BeginSelector("Profile", keys, m_SelectedProfileId))
        {
            ImGui::TextDisabled("No profiles in YAML.");
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
        ImGui::TextDisabled("Visible Systems");
        if (DrawBool(visibleSystems, "bossProtectionHud", "Boss Protection HUD")) m_Dirty = true;
        if (DrawBool(visibleSystems, "combatStateHud", "Combat State HUD")) m_Dirty = true;
        if (DrawBool(visibleSystems, "breakLimitHint", "Break Limit Hint")) m_Dirty = true;
    }

    void SideCombatTuningEditorPanel::DrawRawPreviewTab()
    {
        RefreshRawPreview();
        DrawRawPreview(m_RawPreview);
        ImGui::TextDisabled("Raw preview is generated from structured controls. Save writes this YAML back to disk.");
    }

} // namespace Wheatear
