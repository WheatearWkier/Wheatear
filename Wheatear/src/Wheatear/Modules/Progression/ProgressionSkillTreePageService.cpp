#include "wtpch.h"
#include "ProgressionSkillTreePageService.h"

#include "GameProgress.h"
#include "Wheatear/Core/Application.h"
#include "Wheatear/Core/AssetAliasRegistry.h"
#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Core/Window.h"
#include "Wheatear/Modules/Common/GameplayUILayoutService.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/SceneQueries.h"
#include "Wheatear/Scene/Scene.h"
#include "Wheatear/UI/UIRuntimeTools.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wheatear::ProgressionSkillTreePageService {

        using SceneQueries::FindEntityByName;
        using UIRuntimeTools::SetImageColor;
        using UIRuntimeTools::SetWidgetCenter;
        using UIRuntimeTools::SetWidgetTopLeft;
        using UIRuntimeTools::SetWidgetVisible;

        using GameplayUILayoutService::SetButtonCommand;
        using GameplayUILayoutService::SetPanelClipChildren;
        using GameplayUILayoutService::SetPanelColors;
        struct WidgetRect
        {
            float Left = 0.0f;
            float Right = 0.0f;
            float Top = 0.0f;
            float Bottom = 0.0f;
        };

        struct SkillTreeVisualNode
        {
            std::string Id;
            std::string ParentId;
            glm::vec2 Position = { 0.5f, 0.5f };
        };

        constexpr float kPi = 3.14159265359f;

        static WidgetRect WidgetToRect(const UIWidgetComponent& widget)
        {
            const float halfW = widget.Size.x * 0.5f;
            const float halfH = widget.Size.y * 0.5f;
            float centerX = widget.Position.x;
            float centerY = widget.Position.y;

            switch (widget.Anchor)
            {
            case UIAnchor::TopLeft:
                centerX += halfW;
                centerY += halfH;
                break;
            case UIAnchor::TopCenter:
                centerY += halfH;
                break;
            case UIAnchor::TopRight:
                centerX -= halfW;
                centerY += halfH;
                break;
            case UIAnchor::MiddleLeft:
                centerX += halfW;
                break;
            case UIAnchor::MiddleCenter:
                break;
            case UIAnchor::MiddleRight:
                centerX -= halfW;
                break;
            case UIAnchor::BottomLeft:
                centerX += halfW;
                centerY -= halfH;
                break;
            case UIAnchor::BottomCenter:
                centerY -= halfH;
                break;
            case UIAnchor::BottomRight:
                centerX -= halfW;
                centerY -= halfH;
                break;
            }

            return { centerX - halfW, centerX + halfW, centerY - halfH, centerY + halfH };
        }

        static bool PointInRect(const WidgetRect& rect, float x, float y)
        {
            return x >= rect.Left && x <= rect.Right && y >= rect.Top && y <= rect.Bottom;
        }

        static bool GetMouseNormalized(float& x, float& y)
        {
            const Window& window = Application::Get().GetWindow();
            if (window.GetWidth() == 0 || window.GetHeight() == 0)
                return false;

            x = Input::GetMouseX() / static_cast<float>(window.GetWidth());
            y = Input::GetMouseY() / static_cast<float>(window.GetHeight());
            return x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f;
        }
        static void SetImageTexture(Scene* scene, const std::string& entityName, const std::string& texturePath)
        {
            UIRuntimeTools::SetImageTexture(scene, entityName, texturePath, true);
        }

        static std::string SkillSafeTag(const std::string& id)
        {
            std::string safe = id;
            for (char& c : safe)
            {
                if (c == '-' || c == ':')
                    c = '_';
            }
            return safe;
        }

        static std::string SkillNodeId(const std::string& prefix, int index)
        {
            return prefix + "-" + (index < 10 ? "0" : "") + std::to_string(index);
        }

        static const std::vector<SkillTreeVisualNode>& GetSkillTreeVisualNodes()
        {
            static const std::vector<SkillTreeVisualNode> nodes = []()
            {
                std::vector<SkillTreeVisualNode> result;
                result.push_back({ "magic_sword_core", "", { 0.50f, 0.50f } });

                auto appendBranch = [&result](const std::string& prefix, float baseDegrees, float curveDegrees)
                {
                    std::string parent = "magic_sword_core";
                    for (int i = 1; i <= 12; ++i)
                    {
                        const std::string id = SkillNodeId(prefix, i);
                        const float t = static_cast<float>(i - 1) / 11.0f;
                        const float radius = 0.11f + 0.045f * static_cast<float>(i);
                        const float angle = (baseDegrees + curveDegrees * t) * kPi / 180.0f;
                        const float ringOffset = (i % 3 == 0 ? 0.014f : (i % 3 == 1 ? -0.006f : 0.006f));
                        const glm::vec2 position = {
                            0.50f + std::cos(angle) * (radius + ringOffset),
                            0.50f + std::sin(angle) * (radius + ringOffset)
                        };
                        result.push_back({ id, parent, position });
                        parent = id;
                    }
                };

                appendBranch("ME", -90.0f, 50.0f);
                appendBranch("MA", -18.0f, 52.0f);
                appendBranch("FU", 54.0f, 52.0f);
                appendBranch("MO", 126.0f, 52.0f);
                appendBranch("LI", 198.0f, 50.0f);
                return result;
            }();
            return nodes;
        }

        static const SkillTreeVisualNode* FindSkillTreeVisualNode(const std::string& id)
        {
            const auto& nodes = GetSkillTreeVisualNodes();
            auto it = std::find_if(nodes.begin(), nodes.end(),
                [&](const SkillTreeVisualNode& node) { return node.Id == id; });
            return it == nodes.end() ? nullptr : &(*it);
        }

    void UpdateDrag(Scene* scene)
        {
            Entity panelEntity = FindEntityByName(scene, "SkillTree_NetworkPanel");
            if (!panelEntity || !panelEntity.HasComponent<UIWidgetComponent>())
                return;

            static bool dragging = false;
            static float lastX = 0.0f;
            static float lastY = 0.0f;

            float mouseX = 0.0f;
            float mouseY = 0.0f;
            const bool hasMouse = GetMouseNormalized(mouseX, mouseY);
            const bool pressed = Input::IsMouseButtonPressed(WT_MOUSE_BUTTON_LEFT);
            if (!pressed || !hasMouse)
            {
                dragging = false;
                return;
            }

            const WidgetRect rect = WidgetToRect(panelEntity.GetComponent<UIWidgetComponent>());
            if (!dragging)
            {
                if (!PointInRect(rect, mouseX, mouseY))
                    return;
                dragging = true;
                lastX = mouseX;
                lastY = mouseY;
                return;
            }

            const float width = std::max(0.001f, rect.Right - rect.Left);
            const float height = std::max(0.001f, rect.Bottom - rect.Top);
            auto& state = GameProgress::GetState();
            state.SkillTreePanX = std::clamp(state.SkillTreePanX + (mouseX - lastX) / width, -0.46f, 0.46f);
            state.SkillTreePanY = std::clamp(state.SkillTreePanY + (mouseY - lastY) / height, -0.46f, 0.46f);
            lastX = mouseX;
            lastY = mouseY;
        }

        static glm::vec2 SkillTreeToCanvas(glm::vec2 local, glm::vec2 pan)
        {
            return local + pan;
        }

        static bool SkillTreeRectVisible(glm::vec2 topLeft, glm::vec2 size)
        {
            constexpr float margin = 0.10f;
            return topLeft.x + size.x > -margin && topLeft.x < 1.0f + margin
                && topLeft.y + size.y > -margin && topLeft.y < 1.0f + margin;
        }

        static bool SkillTreeSegmentVisible(glm::vec2 a, glm::vec2 b, float thickness)
        {
            constexpr float margin = 0.10f;
            const glm::vec2 minPoint = { std::min(a.x, b.x) - thickness, std::min(a.y, b.y) - thickness };
            const glm::vec2 maxPoint = { std::max(a.x, b.x) + thickness, std::max(a.y, b.y) + thickness };
            return maxPoint.x > -margin && minPoint.x < 1.0f + margin
                && maxPoint.y > -margin && minPoint.y < 1.0f + margin;
        }

        struct SkillTreeCanvasCache
        {
            Scene* ScenePtr = nullptr;
            uint64_t PanelId = 0;
            float PanX = std::numeric_limits<float>::quiet_NaN();
            float PanY = std::numeric_limits<float>::quiet_NaN();
            std::string SelectedNodeId;
            size_t UnlockedHash = 0;
            bool Initialized = false;
        };

        static SkillTreeCanvasCache s_SkillTreeCanvasCache;

    void ResetCache()
        {
            s_SkillTreeCanvasCache = {};
        }

        template<typename TSkillSet>
        static size_t HashSkillSet(const TSkillSet& skills)
        {
            size_t seed = skills.size();
            for (const auto& skill : skills)
            {
                const size_t value = std::hash<std::string>{}(skill);
                seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
            }
            return seed;
        }

    void UpdateLegacyCanvas(Scene* scene)
        {
            Entity panelEntity = FindEntityByName(scene, "SkillTree_NetworkPanel");
            if (!panelEntity || !panelEntity.HasComponent<UIWidgetComponent>())
                return;

            const auto& state = GameProgress::GetState();
            SetPanelClipChildren(scene, "SkillTree_NetworkPanel", true);
            const glm::vec2 pan = { state.SkillTreePanX, state.SkillTreePanY };
            const uint64_t panelId = static_cast<uint64_t>(panelEntity.GetUUID());
            if (s_SkillTreeCanvasCache.ScenePtr != scene || s_SkillTreeCanvasCache.PanelId != panelId)
            {
                s_SkillTreeCanvasCache = {};
                s_SkillTreeCanvasCache.ScenePtr = scene;
                s_SkillTreeCanvasCache.PanelId = panelId;
            }

            const size_t unlockedHash = HashSkillSet(state.UnlockedSkills);
            const bool dirty = !s_SkillTreeCanvasCache.Initialized
                || std::abs(s_SkillTreeCanvasCache.PanX - pan.x) > 0.0002f
                || std::abs(s_SkillTreeCanvasCache.PanY - pan.y) > 0.0002f
                || s_SkillTreeCanvasCache.SelectedNodeId != state.SelectedSkillNodeId
                || s_SkillTreeCanvasCache.UnlockedHash != unlockedHash;
            if (!dirty)
                return;

            s_SkillTreeCanvasCache.Initialized = true;
            s_SkillTreeCanvasCache.PanX = pan.x;
            s_SkillTreeCanvasCache.PanY = pan.y;
            s_SkillTreeCanvasCache.SelectedNodeId = state.SelectedSkillNodeId;
            s_SkillTreeCanvasCache.UnlockedHash = unlockedHash;

            auto& registry = scene->GetRegistry();
            std::unordered_map<std::string, entt::entity> tags;
            tags.reserve(512);
            for (auto entity : registry.view<TagComponent>())
            {
                const auto& tag = registry.get<TagComponent>(entity).Tag;
                if (!tag.empty())
                    tags[tag] = entity;
            }

            auto findEntity = [&tags](const std::string& entityName) -> entt::entity
            {
                auto it = tags.find(entityName);
                return it != tags.end() ? it->second : entt::null;
            };

            auto setWidgetParent = [&](const std::string& entityName, const std::string& parentTag)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity == entt::null || !registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
                    return;

                const entt::entity parent = findEntity(parentTag);
                auto& widget = registry.get<UIWidgetComponent>(entity);
                widget.ParentEntity = parent != entt::null && registry.valid(parent) && registry.all_of<IDComponent>(parent)
                    ? registry.get<IDComponent>(parent).ID
                    : UUID(0);
            };

            auto setWidgetTopLeft = [&](const std::string& entityName, glm::vec2 position, glm::vec2 size)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity == entt::null || !registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
                    return;

                auto& widget = registry.get<UIWidgetComponent>(entity);
                widget.Anchor = UIAnchor::TopLeft;
                widget.Position = position;
                widget.Size = size;
                widget.Rotation = 0.0f;
            };

            auto setWidgetCenter = [&](const std::string& entityName, glm::vec2 position, glm::vec2 size, float rotation = 0.0f)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity == entt::null || !registry.valid(entity) || !registry.all_of<UIWidgetComponent>(entity))
                    return;

                auto& widget = registry.get<UIWidgetComponent>(entity);
                widget.Anchor = UIAnchor::MiddleCenter;
                widget.Position = position;
                widget.Size = size;
                widget.Rotation = rotation;
            };

            auto setWidgetVisible = [&](const std::string& entityName, bool visible)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity != entt::null && registry.valid(entity) && registry.all_of<UIWidgetComponent>(entity))
                    registry.get<UIWidgetComponent>(entity).Visible = visible;
            };

            auto setImageColor = [&](const std::string& entityName, glm::vec4 color)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity != entt::null && registry.valid(entity) && registry.all_of<UIImageComponent>(entity))
                    registry.get<UIImageComponent>(entity).Color = color;
            };

            auto setPanelColors = [&](const std::string& entityName, glm::vec4 background, glm::vec4 border)
            {
                const entt::entity entity = findEntity(entityName);
                if (entity != entt::null && registry.valid(entity) && registry.all_of<UIPanelComponent>(entity))
                {
                    auto& panel = registry.get<UIPanelComponent>(entity);
                    panel.BackgroundColor = background;
                    panel.BorderColor = border;
                }
            };

            const glm::vec2 nodeSize = { 0.076f, 0.104f };
            const glm::vec2 labelSize = { 0.128f, 0.044f };
            constexpr float lineThickness = 0.0095f;
            constexpr float nodeEdgeInset = 0.050f;

            const auto& nodes = GetSkillTreeVisualNodes();
            for (const auto& node : nodes)
            {
                const std::string safe = SkillSafeTag(node.Id);
                const std::string tag = "SkillTree_Node_" + safe;
                const glm::vec2 center = SkillTreeToCanvas(node.Position, pan);
                const glm::vec2 topLeft = center - nodeSize * 0.5f;
                const bool visible = SkillTreeRectVisible(topLeft, nodeSize);
                const bool learned = state.UnlockedSkills.find(node.Id) != state.UnlockedSkills.end();
                const bool selected = state.SelectedSkillNodeId == node.Id;
                const glm::vec4 iconColor = learned ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
                                                     : glm::vec4(0.32f, 0.35f, 0.36f, 0.90f);

                setWidgetParent(tag, "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Button", "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Lock", "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Selected", "SkillTree_NetworkPanel");
                setWidgetParent(tag + "_Label", "SkillTree_NetworkPanel");

                setWidgetVisible(tag, visible);
                setWidgetVisible(tag + "_Button", visible);
                setWidgetVisible(tag + "_Lock", visible && !learned);
                setWidgetVisible(tag + "_Selected", visible && selected);
                setWidgetVisible(tag + "_Label", visible && selected);
                if (!visible)
                    continue;

                setWidgetTopLeft(tag, topLeft, nodeSize);
                setWidgetTopLeft(tag + "_Button", topLeft, nodeSize);
                setWidgetTopLeft(tag + "_Lock", topLeft, nodeSize);
                setWidgetTopLeft(tag + "_Selected", center - nodeSize * 0.58f, nodeSize * 1.16f);
                setWidgetTopLeft(tag + "_Label", { center.x - labelSize.x * 0.5f, center.y + nodeSize.y * 0.55f }, labelSize);
                setImageColor(tag, iconColor);
            }

            int lineIndex = 1;
            for (const auto& node : nodes)
            {
                if (node.ParentId.empty())
                    continue;

                const SkillTreeVisualNode* parent = FindSkillTreeVisualNode(node.ParentId);
                if (!parent)
                    continue;

                const glm::vec2 parentCenter = SkillTreeToCanvas(parent->Position, pan);
                const glm::vec2 nodeCenter = SkillTreeToCanvas(node.Position, pan);
                const bool active = state.UnlockedSkills.find(parent->Id) != state.UnlockedSkills.end()
                    && state.UnlockedSkills.find(node.Id) != state.UnlockedSkills.end();
                const std::string lineTag = "SkillTree_Line_" + std::to_string(lineIndex++);

                setWidgetParent(lineTag, "SkillTree_NetworkPanel");

                const glm::vec2 delta = nodeCenter - parentCenter;
                const float centerDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (centerDistance <= nodeEdgeInset * 2.0f)
                {
                    setWidgetVisible(lineTag, false);
                    continue;
                }

                const glm::vec2 direction = delta / centerDistance;
                const glm::vec2 a = parentCenter + direction * nodeEdgeInset;
                const glm::vec2 b = nodeCenter - direction * nodeEdgeInset;
                const bool visible = SkillTreeSegmentVisible(a, b, lineThickness);
                if (!visible)
                {
                    setWidgetVisible(lineTag, false);
                    continue;
                }

                const float length = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
                const float angle = std::atan2(b.y - a.y, b.x - a.x) * 57.2957795f;

                setWidgetCenter(lineTag, (a + b) * 0.5f, { length, lineThickness }, angle);
                setWidgetVisible(lineTag, true);
                setPanelColors(lineTag,
                    active ? glm::vec4(0.38f, 0.96f, 0.72f, 0.78f) : glm::vec4(0.18f, 0.34f, 0.30f, 0.54f),
                    glm::vec4(0.0f));
            }
        }

        static std::string SkillTreeIconPath(const std::string& nodeId)
        {
            if (nodeId == "magic_sword_core")
                return AssetAliasRegistry::Path("progression.skill_tree.magic_sword_core");
            return AssetAliasRegistry::Path("progression.skill_tree.root")
                + "skill_" + SkillSafeTag(nodeId) + ".png";
        }

        static std::string SkillTreeBranchName(const std::string& nodeId)
        {
            if (nodeId == "magic_sword_core") return "核心";
            if (nodeId.rfind("ME-", 0) == 0) return "近战";
            if (nodeId.rfind("MA-", 0) == 0) return "魔法";
            if (nodeId.rfind("FU-", 0) == 0) return "空中";
            if (nodeId.rfind("MO-", 0) == 0) return "机动";
            if (nodeId.rfind("LI-", 0) == 0) return "断限";
            return "未知";
        }

        static int SkillTreeUnlockChapter(const std::string& nodeId)
        {
            if (nodeId == "magic_sword_core")
                return 1;

            const size_t separator = nodeId.find('-');
            if (separator == std::string::npos || separator + 1 >= nodeId.size())
                return 1;

            int index = 1;
            try
            {
                index = std::max(std::stoi(nodeId.substr(separator + 1)), 1);
            }
            catch (...)
            {
                index = 1;
            }

            if (index <= 2) return 2;
            if (index <= 4) return 3;
            if (index <= 6) return 7;
            if (index <= 8) return 10;
            if (index <= 10) return 13;
            return 17;
        }

        static bool SkillTreeViewNeedsRebuild(const UISkillTreeViewComponent& tree,
            const std::vector<SkillTreeVisualNode>& visualNodes)
        {
            if (tree.Nodes.size() != visualNodes.size())
                return true;

            for (size_t i = 0; i < visualNodes.size(); ++i)
            {
                if (tree.Nodes[i].Id != visualNodes[i].Id
                    || tree.Nodes[i].ParentId != visualNodes[i].ParentId)
                    return true;
            }
            return false;
        }

        static void PopulateSkillTreeViewNodes(UISkillTreeViewComponent& tree)
        {
            const auto& visualNodes = GetSkillTreeVisualNodes();
            tree.Nodes.clear();
            tree.Nodes.reserve(visualNodes.size());
            for (const auto& visualNode : visualNodes)
            {
                UISkillTreeNodeView node;
                node.Id = visualNode.Id;
                node.ParentId = visualNode.ParentId;
                node.Position = visualNode.Position;
                node.IconPath = SkillTreeIconPath(visualNode.Id);
                node.Branch = SkillTreeBranchName(visualNode.Id);
                node.UnlockChapter = SkillTreeUnlockChapter(visualNode.Id);
                tree.Nodes.push_back(node);
            }
        }

        static void HideLegacySkillTreeEntities(Scene* scene)
        {
            if (!scene)
                return;

            auto& registry = scene->GetRegistry();
            for (auto e : registry.view<TagComponent, UIWidgetComponent>())
            {
                const auto& tag = registry.get<TagComponent>(e).Tag;
                if (tag.rfind("SkillTree_Node_", 0) == 0
                    || tag.rfind("SkillTree_Line_", 0) == 0)
                {
                    registry.get<UIWidgetComponent>(e).Visible = false;
                }
            }
        }

    bool SyncView(Scene* scene)
        {
            Entity treeEntity = FindEntityByName(scene, "SkillTree_View");
            if (!treeEntity)
                treeEntity = FindEntityByName(scene, "SkillTree_NetworkPanel");
            if (!treeEntity || !treeEntity.HasComponent<UIWidgetComponent>())
                return false;

            auto& tree = treeEntity.HasComponent<UISkillTreeViewComponent>()
                ? treeEntity.GetComponent<UISkillTreeViewComponent>()
                : treeEntity.AddComponent<UISkillTreeViewComponent>();

            if (treeEntity.HasComponent<UIPanelComponent>())
                treeEntity.GetComponent<UIPanelComponent>().ClipChildren = true;

            const auto& visualNodes = GetSkillTreeVisualNodes();
            if (SkillTreeViewNeedsRebuild(tree, visualNodes))
                PopulateSkillTreeViewNodes(tree);

            auto& state = GameProgress::GetState();
            if (!tree.RuntimeDragging)
            {
                tree.Pan = { state.SkillTreePanX, state.SkillTreePanY };
                tree.ClampPan();
            }

            tree.SelectedNodeId = state.SelectedSkillNodeId;
            tree.CommandPrefix = tree.CommandPrefix.empty()
                ? "progression:select_skill_node:"
                : tree.CommandPrefix;

            for (auto& node : tree.Nodes)
            {
                node.IconPath = node.IconPath.empty() ? SkillTreeIconPath(node.Id) : node.IconPath;
                node.Branch = node.Branch.empty() ? SkillTreeBranchName(node.Id) : node.Branch;
                node.UnlockChapter = node.UnlockChapter <= 0 ? SkillTreeUnlockChapter(node.Id) : node.UnlockChapter;
                node.Learned = node.Id == "magic_sword_core"
                    || state.UnlockedSkills.find(node.Id) != state.UnlockedSkills.end();
                node.Available = node.UnlockChapter <= state.CurrentChapter;
                node.Locked = !node.Learned;
                node.Selected = node.Id == state.SelectedSkillNodeId;
            }

            HideLegacySkillTreeEntities(scene);
            return true;
        }


} // namespace Wheatear::ProgressionSkillTreePageService
