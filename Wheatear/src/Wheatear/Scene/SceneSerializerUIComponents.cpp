#include "wtpch.h"
#include "SceneSerializerComponentSupport.h"

namespace Wheatear {

    template<> struct ComponentSerializer<UICanvasComponent> {
        static constexpr const char* Key = "UICanvasComponent";
        static void Serialize(YAML::Emitter& o, const UICanvasComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Visible" << YAML::Value << c.Visible;
            o << YAML::Key << "ReferenceWidth" << YAML::Value << c.ReferenceWidth;
            o << YAML::Key << "ReferenceHeight" << YAML::Value << c.ReferenceHeight;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UICanvasComponent& c) {
            c.Visible = n["Visible"].as<bool>();
            c.ReferenceWidth = n["ReferenceWidth"].as<float>();
            c.ReferenceHeight = n["ReferenceHeight"].as<float>();
        }
    };

    template<> struct ComponentSerializer<UIWidgetComponent> {
        static constexpr const char* Key = "UIWidgetComponent";
        static void Serialize(YAML::Emitter& o, const UIWidgetComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Visible" << YAML::Value << c.Visible;
            o << YAML::Key << "Position" << YAML::Value << c.Position;
            o << YAML::Key << "Size" << YAML::Value << c.Size;
            o << YAML::Key << "Rotation" << YAML::Value << c.Rotation;
            o << YAML::Key << "Anchor" << YAML::Value << (int)c.Anchor;
            o << YAML::Key << "SortOrder" << YAML::Value << c.SortOrder;
            o << YAML::Key << "ParentEntity" << YAML::Value << static_cast<uint64_t>(c.ParentEntity);
            o << YAML::Key << "ParentTag" << YAML::Value << c.ParentTag;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIWidgetComponent& c) {
            c.Visible = n["Visible"].as<bool>();
            c.Position = n["Position"].as<glm::vec2>();
            c.Size = n["Size"].as<glm::vec2>();
            c.Rotation = n["Rotation"].as<float>();
            c.Anchor = (UIAnchor)n["Anchor"].as<int>();
            c.SortOrder = n["SortOrder"].as<int>();
            c.ParentEntity = UUID(n["ParentEntity"].as<uint64_t>(static_cast<uint64_t>(c.ParentEntity)));
            c.ParentTag = n["ParentTag"].as<std::string>(c.ParentTag);
        }
    };

    template<> struct ComponentSerializer<UIAnimatorComponent> {
        static constexpr const char* Key = "UIAnimatorComponent";
        static void Serialize(YAML::Emitter& o, const UIAnimatorComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Preset" << YAML::Value << YAML::DoubleQuoted << c.Preset;
            o << YAML::Key << "PlayOnStart" << YAML::Value << c.PlayOnStart;
            o << YAML::Key << "Loop" << YAML::Value << c.Loop;
            o << YAML::Key << "Delay" << YAML::Value << c.Delay;
            o << YAML::Key << "Duration" << YAML::Value << c.Duration;
            o << YAML::Key << "Amplitude" << YAML::Value << c.Amplitude;
            o << YAML::Key << "Speed" << YAML::Value << c.Speed;
            o << YAML::Key << "FromOffset" << YAML::Value << c.FromOffset;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIAnimatorComponent& c) {
            c.Preset = n["Preset"].as<std::string>(c.Preset);
            c.PlayOnStart = n["PlayOnStart"].as<bool>(c.PlayOnStart);
            c.Loop = n["Loop"].as<bool>(c.Loop);
            c.Delay = n["Delay"].as<float>(c.Delay);
            c.Duration = n["Duration"].as<float>(c.Duration);
            c.Amplitude = n["Amplitude"].as<float>(c.Amplitude);
            c.Speed = n["Speed"].as<float>(c.Speed);
            c.FromOffset = n["FromOffset"].as<glm::vec2>(c.FromOffset);
        }
    };

    template<> struct ComponentSerializer<UIImageComponent> {
        static constexpr const char* Key = "UIImageComponent";
        static void Serialize(YAML::Emitter& o, const UIImageComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "TexturePath" << YAML::Value << (c.Texture ? c.Texture->GetPath() : "");
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIImageComponent& c) {
            c.Color = n["Color"].as<glm::vec4>();
            if (auto p = n["TexturePath"].as<std::string>(""); !p.empty())
                c.Texture = Texture2D::Create(p);
        }
    };
    template<> struct ComponentSerializer<UIPanelComponent> {
        static constexpr const char* Key = "UIPanelComponent";
        static void Serialize(YAML::Emitter& o, const UIPanelComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "BackgroundColor" << YAML::Value << c.BackgroundColor;
            o << YAML::Key << "BorderColor" << YAML::Value << c.BorderColor;
            o << YAML::Key << "BorderThickness" << YAML::Value << c.BorderThickness;
            o << YAML::Key << "ClipChildren" << YAML::Value << c.ClipChildren;
            o << YAML::Key << "Draggable" << YAML::Value << c.Draggable;
            o << YAML::Key << "ConstrainDragToParent" << YAML::Value << c.ConstrainDragToParent;
            o << YAML::Key << "DragHandleHeight" << YAML::Value << c.DragHandleHeight;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIPanelComponent& c) {
            c.BackgroundColor = n["BackgroundColor"].as<glm::vec4>(c.BackgroundColor);
            c.BorderColor = n["BorderColor"].as<glm::vec4>(c.BorderColor);
            c.BorderThickness = n["BorderThickness"].as<float>(c.BorderThickness);
            c.ClipChildren = n["ClipChildren"].as<bool>(c.ClipChildren);
            c.Draggable = n["Draggable"].as<bool>(c.Draggable);
            c.ConstrainDragToParent = n["ConstrainDragToParent"].as<bool>(c.ConstrainDragToParent);
            c.DragHandleHeight = n["DragHandleHeight"].as<float>(c.DragHandleHeight);
        }
    };

    template<> struct ComponentSerializer<UITextComponent> {
        static constexpr const char* Key = "UITextComponent";
        static void Serialize(YAML::Emitter& o, const UITextComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Text" << YAML::Value << YAML::DoubleQuoted << c.Text;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "FontSize" << YAML::Value << c.FontSize;
            o << YAML::Key << "FontPath" << YAML::Value << c.FontPath;
            o << YAML::Key << "ShadowColor" << YAML::Value << c.ShadowColor;
            o << YAML::Key << "ShadowOffset" << YAML::Value << c.ShadowOffset;
            o << YAML::Key << "OutlineColor" << YAML::Value << c.OutlineColor;
            o << YAML::Key << "OutlineThickness" << YAML::Value << c.OutlineThickness;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UITextComponent& c) {
            c.Text = n["Text"].as<std::string>(c.Text);
            c.Color = n["Color"].as<glm::vec4>(c.Color);
            c.FontSize = n["FontSize"].as<float>(c.FontSize);
            c.FontPath = n["FontPath"].as<std::string>(c.FontPath);
            c.ShadowColor = n["ShadowColor"].as<glm::vec4>(c.ShadowColor);
            c.ShadowOffset = n["ShadowOffset"].as<glm::vec2>(c.ShadowOffset);
            c.OutlineColor = n["OutlineColor"].as<glm::vec4>(c.OutlineColor);
            c.OutlineThickness = n["OutlineThickness"].as<float>(c.OutlineThickness);
        }
    };

    template<> struct ComponentSerializer<UIButtonComponent> {
        static constexpr const char* Key = "UIButtonComponent";
        static void Serialize(YAML::Emitter& o, const UIButtonComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "NormalColor" << YAML::Value << c.NormalColor;
            o << YAML::Key << "HoverColor" << YAML::Value << c.HoverColor;
            o << YAML::Key << "PressedColor" << YAML::Value << c.PressedColor;
            o << YAML::Key << "OnClickFunction" << YAML::Value << YAML::DoubleQuoted << c.OnClickFunction;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIButtonComponent& c) {
            c.NormalColor = n["NormalColor"].as<glm::vec4>();
            c.HoverColor = n["HoverColor"].as<glm::vec4>();
            c.PressedColor = n["PressedColor"].as<glm::vec4>();
            c.OnClickFunction = n["OnClickFunction"].as<std::string>();
        }
    };

    template<> struct ComponentSerializer<UIProgressBarComponent> {
        static constexpr const char* Key = "UIProgressBarComponent";
        static void Serialize(YAML::Emitter& o, const UIProgressBarComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Value" << YAML::Value << c.Value;
            o << YAML::Key << "MaxValue" << YAML::Value << c.MaxValue;
            o << YAML::Key << "ForegroundColor" << YAML::Value << c.ForegroundColor;
            o << YAML::Key << "BackgroundColor" << YAML::Value << c.BackgroundColor;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIProgressBarComponent& c) {
            c.Value = n["Value"].as<float>();
            c.MaxValue = n["MaxValue"].as<float>();
            c.ForegroundColor = n["ForegroundColor"].as<glm::vec4>();
            c.BackgroundColor = n["BackgroundColor"].as<glm::vec4>();
        }
    };
    template<> struct ComponentSerializer<UISliderComponent> {
        static constexpr const char* Key = "UISliderComponent";
        static void Serialize(YAML::Emitter& o, const UISliderComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Value" << YAML::Value << c.Value;
            o << YAML::Key << "MinValue" << YAML::Value << c.MinValue;
            o << YAML::Key << "MaxValue" << YAML::Value << c.MaxValue;
            o << YAML::Key << "TrackColor" << YAML::Value << c.TrackColor;
            o << YAML::Key << "FillColor" << YAML::Value << c.FillColor;
            o << YAML::Key << "HandleColor" << YAML::Value << c.HandleColor;
            o << YAML::Key << "HoverColor" << YAML::Value << c.HoverColor;
            o << YAML::Key << "OnValueChangedFunction" << YAML::Value << YAML::DoubleQuoted << c.OnValueChangedFunction;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UISliderComponent& c) {
            c.Value = n["Value"].as<float>(c.Value);
            c.MinValue = n["MinValue"].as<float>(c.MinValue);
            c.MaxValue = n["MaxValue"].as<float>(c.MaxValue);
            c.TrackColor = n["TrackColor"].as<glm::vec4>(c.TrackColor);
            c.FillColor = n["FillColor"].as<glm::vec4>(c.FillColor);
            c.HandleColor = n["HandleColor"].as<glm::vec4>(c.HandleColor);
            c.HoverColor = n["HoverColor"].as<glm::vec4>(c.HoverColor);
            c.OnValueChangedFunction = n["OnValueChangedFunction"].as<std::string>(c.OnValueChangedFunction);
        }
    };

    template<> struct ComponentSerializer<UIPagerComponent> {
        static constexpr const char* Key = "UIPagerComponent";
        static void Serialize(YAML::Emitter& o, const UIPagerComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "CurrentPage" << YAML::Value << c.CurrentPage;
            o << YAML::Key << "PageCount" << YAML::Value << c.PageCount;
            o << YAML::Key << "Wrap" << YAML::Value << c.Wrap;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIPagerComponent& c) {
            c.CurrentPage = n["CurrentPage"].as<int>(c.CurrentPage);
            c.PageCount = n["PageCount"].as<int>(c.PageCount);
            c.Wrap = n["Wrap"].as<bool>(c.Wrap);
            c.PageCount = std::max(c.PageCount, 1);
            c.CurrentPage = std::clamp(c.CurrentPage, 1, c.PageCount);
        }
    };

    template<> struct ComponentSerializer<UIScrollViewComponent> {
        static constexpr const char* Key = "UIScrollViewComponent";
        static void Serialize(YAML::Emitter& o, const UIScrollViewComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "OffsetY" << YAML::Value << c.OffsetY;
            o << YAML::Key << "ContentHeight" << YAML::Value << c.ContentHeight;
            o << YAML::Key << "WheelStep" << YAML::Value << c.WheelStep;
            o << YAML::Key << "ScrollbarWidth" << YAML::Value << c.ScrollbarWidth;
            o << YAML::Key << "EnableWheel" << YAML::Value << c.EnableWheel;
            o << YAML::Key << "ShowScrollbar" << YAML::Value << c.ShowScrollbar;
            o << YAML::Key << "DragScrollbar" << YAML::Value << c.DragScrollbar;
            o << YAML::Key << "ClampToContent" << YAML::Value << c.ClampToContent;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIScrollViewComponent& c) {
            c.OffsetY = n["OffsetY"].as<float>(c.OffsetY);
            c.ContentHeight = n["ContentHeight"].as<float>(c.ContentHeight);
            c.WheelStep = n["WheelStep"].as<float>(c.WheelStep);
            c.ScrollbarWidth = n["ScrollbarWidth"].as<float>(c.ScrollbarWidth);
            c.EnableWheel = n["EnableWheel"].as<bool>(c.EnableWheel);
            c.ShowScrollbar = n["ShowScrollbar"].as<bool>(c.ShowScrollbar);
            c.DragScrollbar = n["DragScrollbar"].as<bool>(c.DragScrollbar);
            c.ClampToContent = n["ClampToContent"].as<bool>(c.ClampToContent);
            c.ClampOffset();
        }
    };

    template<> struct ComponentSerializer<UIPathComponent> {
        static constexpr const char* Key = "UIPathComponent";
        static void Serialize(YAML::Emitter& o, const UIPathComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Mode" << YAML::Value << static_cast<int>(c.Mode);
            o << YAML::Key << "Points" << YAML::Value << YAML::BeginSeq;
            for (const auto& point : c.Points)
                o << point;
            o << YAML::EndSeq;
            o << YAML::Key << "Color" << YAML::Value << c.Color;
            o << YAML::Key << "GlowColor" << YAML::Value << c.GlowColor;
            o << YAML::Key << "Thickness" << YAML::Value << c.Thickness;
            o << YAML::Key << "GlowThicknessMultiplier" << YAML::Value << c.GlowThicknessMultiplier;
            o << YAML::Key << "Segments" << YAML::Value << c.Segments;
            o << YAML::Key << "Closed" << YAML::Value << c.Closed;
            o << YAML::Key << "DrawGlow" << YAML::Value << c.DrawGlow;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIPathComponent& c) {
            c.Mode = static_cast<UIPathMode>(std::clamp(n["Mode"].as<int>(static_cast<int>(c.Mode)), 0, 2));
            if (auto points = n["Points"])
            {
                c.Points.clear();
                for (auto point : points)
                    c.Points.push_back(point.as<glm::vec2>());
            }
            c.Color = n["Color"].as<glm::vec4>(c.Color);
            c.GlowColor = n["GlowColor"].as<glm::vec4>(c.GlowColor);
            c.Thickness = n["Thickness"].as<float>(c.Thickness);
            c.GlowThicknessMultiplier = n["GlowThicknessMultiplier"].as<float>(c.GlowThicknessMultiplier);
            c.Segments = std::clamp(n["Segments"].as<int>(c.Segments), 2, 96);
            c.Closed = n["Closed"].as<bool>(c.Closed);
            c.DrawGlow = n["DrawGlow"].as<bool>(c.DrawGlow);
        }
    };

    template<> struct ComponentSerializer<UISkillTreeViewComponent> {
        static constexpr const char* Key = "UISkillTreeViewComponent";
        static void Serialize(YAML::Emitter& o, const UISkillTreeViewComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Pan" << YAML::Value << c.Pan;
            o << YAML::Key << "MinPan" << YAML::Value << c.MinPan;
            o << YAML::Key << "MaxPan" << YAML::Value << c.MaxPan;
            o << YAML::Key << "NodeSize" << YAML::Value << c.NodeSize;
            o << YAML::Key << "NodeEdgeInset" << YAML::Value << c.NodeEdgeInset;
            o << YAML::Key << "LineThickness" << YAML::Value << c.LineThickness;
            o << YAML::Key << "CurveAmount" << YAML::Value << c.CurveAmount;
            o << YAML::Key << "VirtualizationMargin" << YAML::Value << c.VirtualizationMargin;
            o << YAML::Key << "LineSegments" << YAML::Value << c.LineSegments;
            o << YAML::Key << "BackgroundRingCount" << YAML::Value << c.BackgroundRingCount;
            o << YAML::Key << "DrawLineGlow" << YAML::Value << c.DrawLineGlow;
            o << YAML::Key << "CommandPrefix" << YAML::Value << YAML::DoubleQuoted << c.CommandPrefix;
            o << YAML::Key << "BackgroundColor" << YAML::Value << c.BackgroundColor;
            o << YAML::Key << "GridColor" << YAML::Value << c.GridColor;
            o << YAML::Key << "LineColor" << YAML::Value << c.LineColor;
            o << YAML::Key << "ActiveLineColor" << YAML::Value << c.ActiveLineColor;
            o << YAML::Key << "LineGlowColor" << YAML::Value << c.LineGlowColor;
            o << YAML::Key << "NodeColor" << YAML::Value << c.NodeColor;
            o << YAML::Key << "LockedNodeColor" << YAML::Value << c.LockedNodeColor;
            o << YAML::Key << "HoverNodeColor" << YAML::Value << c.HoverNodeColor;
            o << YAML::Key << "SelectedNodeColor" << YAML::Value << c.SelectedNodeColor;
            o << YAML::Key << "CoreNodeColor" << YAML::Value << c.CoreNodeColor;
            o << YAML::Key << "LockColor" << YAML::Value << c.LockColor;
            o << YAML::Key << "SelectedNodeId" << YAML::Value << YAML::DoubleQuoted << c.SelectedNodeId;
            o << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
            for (const auto& node : c.Nodes)
            {
                o << YAML::BeginMap;
                o << YAML::Key << "Id" << YAML::Value << YAML::DoubleQuoted << node.Id;
                o << YAML::Key << "ParentId" << YAML::Value << YAML::DoubleQuoted << node.ParentId;
                o << YAML::Key << "Position" << YAML::Value << node.Position;
                o << YAML::Key << "IconPath" << YAML::Value << YAML::DoubleQuoted << node.IconPath;
                o << YAML::Key << "Branch" << YAML::Value << YAML::DoubleQuoted << node.Branch;
                o << YAML::Key << "UnlockChapter" << YAML::Value << node.UnlockChapter;
                o << YAML::Key << "Learned" << YAML::Value << node.Learned;
                o << YAML::Key << "Available" << YAML::Value << node.Available;
                o << YAML::Key << "Selected" << YAML::Value << node.Selected;
                o << YAML::Key << "Locked" << YAML::Value << node.Locked;
                o << YAML::EndMap;
            }
            o << YAML::EndSeq;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UISkillTreeViewComponent& c) {
            c.Pan = n["Pan"].as<glm::vec2>(c.Pan);
            c.MinPan = n["MinPan"].as<glm::vec2>(c.MinPan);
            c.MaxPan = n["MaxPan"].as<glm::vec2>(c.MaxPan);
            c.NodeSize = n["NodeSize"].as<glm::vec2>(c.NodeSize);
            c.NodeEdgeInset = n["NodeEdgeInset"].as<float>(c.NodeEdgeInset);
            c.LineThickness = n["LineThickness"].as<float>(c.LineThickness);
            c.CurveAmount = n["CurveAmount"].as<float>(c.CurveAmount);
            c.VirtualizationMargin = n["VirtualizationMargin"].as<float>(c.VirtualizationMargin);
            c.LineSegments = std::clamp(n["LineSegments"].as<int>(c.LineSegments), 2, 96);
            c.BackgroundRingCount = std::clamp(n["BackgroundRingCount"].as<int>(c.BackgroundRingCount), 0, 8);
            c.DrawLineGlow = n["DrawLineGlow"].as<bool>(c.DrawLineGlow);
            c.CommandPrefix = n["CommandPrefix"].as<std::string>(c.CommandPrefix);
            c.BackgroundColor = n["BackgroundColor"].as<glm::vec4>(c.BackgroundColor);
            c.GridColor = n["GridColor"].as<glm::vec4>(c.GridColor);
            c.LineColor = n["LineColor"].as<glm::vec4>(c.LineColor);
            c.ActiveLineColor = n["ActiveLineColor"].as<glm::vec4>(c.ActiveLineColor);
            c.LineGlowColor = n["LineGlowColor"].as<glm::vec4>(c.LineGlowColor);
            c.NodeColor = n["NodeColor"].as<glm::vec4>(c.NodeColor);
            c.LockedNodeColor = n["LockedNodeColor"].as<glm::vec4>(c.LockedNodeColor);
            c.HoverNodeColor = n["HoverNodeColor"].as<glm::vec4>(c.HoverNodeColor);
            c.SelectedNodeColor = n["SelectedNodeColor"].as<glm::vec4>(c.SelectedNodeColor);
            c.CoreNodeColor = n["CoreNodeColor"].as<glm::vec4>(c.CoreNodeColor);
            c.LockColor = n["LockColor"].as<glm::vec4>(c.LockColor);
            c.SelectedNodeId = n["SelectedNodeId"].as<std::string>(c.SelectedNodeId);
            if (auto nodes = n["Nodes"])
            {
                c.Nodes.clear();
                for (auto nodeData : nodes)
                {
                    UISkillTreeNodeView node;
                    node.Id = nodeData["Id"].as<std::string>(node.Id);
                    node.ParentId = nodeData["ParentId"].as<std::string>(node.ParentId);
                    node.Position = nodeData["Position"].as<glm::vec2>(node.Position);
                    node.IconPath = nodeData["IconPath"].as<std::string>(node.IconPath);
                    node.Branch = nodeData["Branch"].as<std::string>(node.Branch);
                    node.UnlockChapter = nodeData["UnlockChapter"].as<int>(node.UnlockChapter);
                    node.Learned = nodeData["Learned"].as<bool>(node.Learned);
                    node.Available = nodeData["Available"].as<bool>(node.Available);
                    node.Selected = nodeData["Selected"].as<bool>(node.Selected);
                    node.Locked = nodeData["Locked"].as<bool>(node.Locked);
                    c.Nodes.push_back(node);
                }
            }
            c.RuntimeHoveredNodeId.clear();
            c.RuntimeDragging = false;
            c.RuntimeDragDistance = 0.0f;
            c.ClampPan();
        }
    };

    template<> struct ComponentSerializer<UIPageItemComponent> {
        static constexpr const char* Key = "UIPageItemComponent";
        static void Serialize(YAML::Emitter& o, const UIPageItemComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "PagerEntity" << YAML::Value << static_cast<uint64_t>(c.PagerEntity);
            o << YAML::Key << "PagerTag" << YAML::Value << YAML::DoubleQuoted << c.PagerTag;
            o << YAML::Key << "Page" << YAML::Value << c.Page;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UIPageItemComponent& c) {
            c.PagerEntity = UUID(n["PagerEntity"].as<uint64_t>(static_cast<uint64_t>(c.PagerEntity)));
            c.PagerTag = n["PagerTag"].as<std::string>(c.PagerTag);
            c.Page = std::max(n["Page"].as<int>(c.Page), 1);
        }
    };

    template<> struct ComponentSerializer<UICheckboxComponent> {
        static constexpr const char* Key = "UICheckboxComponent";
        static void Serialize(YAML::Emitter& o, const UICheckboxComponent& c) {
            o << YAML::Key << Key << YAML::BeginMap;
            o << YAML::Key << "Checked" << YAML::Value << c.Checked;
            o << YAML::Key << "BoxColor" << YAML::Value << c.BoxColor;
            o << YAML::Key << "CheckColor" << YAML::Value << c.CheckColor;
            o << YAML::Key << "HoverColor" << YAML::Value << c.HoverColor;
            o << YAML::Key << "PressedColor" << YAML::Value << c.PressedColor;
            o << YAML::Key << "OnValueChangedFunction" << YAML::Value << YAML::DoubleQuoted << c.OnValueChangedFunction;
            o << YAML::EndMap;
        }
        static void Deserialize(const YAML::Node& n, UICheckboxComponent& c) {
            c.Checked = n["Checked"].as<bool>(c.Checked);
            c.BoxColor = n["BoxColor"].as<glm::vec4>(c.BoxColor);
            c.CheckColor = n["CheckColor"].as<glm::vec4>(c.CheckColor);
            c.HoverColor = n["HoverColor"].as<glm::vec4>(c.HoverColor);
            c.PressedColor = n["PressedColor"].as<glm::vec4>(c.PressedColor);
            c.OnValueChangedFunction = n["OnValueChangedFunction"].as<std::string>(c.OnValueChangedFunction);
        }
    };

    using UISceneComponents = ComponentGroup
    <
        UICanvasComponent,
        UIWidgetComponent,
        UIAnimatorComponent,
        UIImageComponent,
        UIPanelComponent,
        UITextComponent,
        UIButtonComponent,
        UIProgressBarComponent,
        UISliderComponent,
        UIPagerComponent,
        UIScrollViewComponent,
        UIPathComponent,
        UISkillTreeViewComponent,
        UIPageItemComponent,
        UICheckboxComponent
    >;

    void SerializeUISceneComponents(YAML::Emitter& out, Entity entity)
    {
        SerializeComponents(UISceneComponents{}, out, entity);
    }

    void DeserializeUISceneComponents(const YAML::Node& node, Entity entity)
    {
        DeserializeComponents(UISceneComponents{}, node, entity);
    }

} // namespace Wheatear
