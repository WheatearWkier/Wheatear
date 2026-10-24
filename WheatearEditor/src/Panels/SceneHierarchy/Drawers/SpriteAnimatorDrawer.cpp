#include "wepch.h"
#include "SpriteAnimatorDrawer.h"
#include "../ComponentDrawers.h"
#include "Editor/EditorLocale.h"
#include "Editor/EditorWidgets.h"
#include "Wheatear/Animation/AnimationClip.h"
#include "Wheatear/Animation/AnimationClipSerializer.h"
#include "Wheatear/Assets/AssetPath.h"
#include <imgui/imgui.h>
#include "Wheatear/Scene/Components.h"

namespace Wheatear {

    void DrawSpriteAnimatorComponent(Entity entity)
    {
        DrawComponent<SpriteAnimatorComponent>("Sprite Animator", entity, [](auto& c)
            {
                ImGui::Text("Clip: %s",
                    c.CurrentClipName.empty() ? "(none)" : c.CurrentClipName.c_str());

                ImGui::SameLine();
                if (c.IsPlaying)
                {
                    if (ImGui::SmallButton(EditorLocale::Text("Pause", "暂停")))  c.IsPlaying = false;
                }
                else
                {
                    if (ImGui::SmallButton(EditorLocale::Text("Resume", "继续"))) c.IsPlaying = true;
                }

                ImGui::Checkbox("Play On Start", &c.PlayOnStart);
                ImGui::SameLine();
                ImGui::Checkbox(EditorLocale::Text("Fire Events", "触发事件"), &c.FireEvents);
                ImGui::DragFloat(EditorLocale::Text("Playback Speed", "播放速度"), &c.PlaybackSpeed, 0.01f, 0.0f, 8.0f);

                ImGui::Text(EditorLocale::Text("Default: %s", "默认: %s"),
                    c.DefaultClipName.empty() ? "(none)" : c.DefaultClipName.c_str());
                ImGui::Text(EditorLocale::Text("Time: %.3fs  Frame: %d  Finished: %s", "时间: %.3fs  帧: %d  结束: %s"),
                    c.ElapsedTime,
                    c.CurrentFrameIndex,
                    c.IsFinished ? "true" : "false");

                ImGui::TextDisabled("%d clip(s) - edit in Animation Editor window",
                    (int)c.Clips.size());

                if (!c.ExternalClipAssets.empty())
                {
                    ImGui::Separator();
                    ImGui::TextDisabled(EditorLocale::Text("External .wtanim assets", "外部 .wtanim 动画资产"));
                    for (const auto& [name, path] : c.ExternalClipAssets)
                        ImGui::BulletText("%s -> %s", name.c_str(), path.c_str());
                }

                // Quick-bind a .wtanim asset to this entity without opening
                // the Animation Editor: picking a file loads it into Clips
                // and records the (name -> asset) binding.
                ImGui::Separator();
                static std::string quickClipPath;
                if (EditorWidgets::DrawAssetReferenceField(
                        EditorLocale::Text("Load .wtanim", "加载 .wtanim"),
                        quickClipPath, EditorWidgets::AssetReferenceKind::AnimationClip, 512))
                {
                    Ref<AnimationClip> loaded =
                        AnimationClipSerializer::Load(AssetPath::Resolve(quickClipPath));
                    if (loaded)
                    {
                        c.AddClip(loaded);
                        c.BindExternalClipAsset(loaded->GetName(), quickClipPath);
                        if (c.DefaultClipName.empty())
                            c.DefaultClipName = loaded->GetName();
                        c.Play(loaded->GetName());
                        WT_CORE_INFO("Loaded animation clip asset: {}", quickClipPath);
                    }
                    else
                    {
                        WT_CORE_ERROR("Failed to load animation clip asset: {}", quickClipPath);
                    }
                }
                EditorWidgets::HelpTooltip(EditorLocale::Text(
                    "Loads a .wtanim clip and binds it to this entity. The same asset can drive any number of entities.",
                    "加载 .wtanim 片段并绑定到当前实体。同一资产可驱动任意多个实体。"));
            });
    }

    void DrawAnimator3DComponent(Entity entity)
    {
        DrawComponent<Animator3DComponent>("Animator 3D", entity, [entity](auto& c)
            {
                Ref<Model> model;
                if (entity.HasComponent<MeshRendererComponent>())
                    model = entity.GetComponent<MeshRendererComponent>().Model;

                if (!model || !model->HasSkin())
                {
                    ImGui::TextDisabled(EditorLocale::Text(
                        "Assign a skinned FBX/glTF model to the Mesh Renderer first.",
                        "请先给 Mesh Renderer 指定带骨骼的 FBX/glTF 模型。"));
                    return;
                }

                const auto& animations = model->GetAnimations();
                if (animations.empty())
                {
                    ImGui::TextDisabled(EditorLocale::Text(
                        "This model has a skeleton but no animation clips.",
                        "该模型有骨骼但没有动画片段。"));
                    return;
                }

                const char* preview = c.DefaultClip.empty()
                    ? "(first clip)" : c.DefaultClip.c_str();
                if (ImGui::BeginCombo(EditorLocale::Text("Default Clip", "默认片段"), preview))
                {
                    for (const ModelAnimation& animation : animations)
                    {
                        const bool selected = animation.Name == c.DefaultClip;
                        if (ImGui::Selectable(animation.Name.c_str(), selected))
                        {
                            c.DefaultClip = animation.Name;
                            c.CurrentClipIndex = -1;
                            c.NextClipIndex = -1;
                            c.Time = 0.0f;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button(c.IsPlaying
                        ? EditorLocale::Text("Pause", "暂停")
                        : EditorLocale::Text("Play", "播放")))
                {
                    c.IsPlaying = !c.IsPlaying;
                }
                ImGui::SameLine();
                if (ImGui::Button(EditorLocale::Text("Restart", "重播")))
                {
                    c.Time = 0.0f;
                    c.NextClipIndex = -1;
                    c.IsPlaying = true;
                }

                ImGui::Checkbox(EditorLocale::Text("Loop", "循环"), &c.Loop);
                ImGui::SameLine();
                ImGui::Checkbox(EditorLocale::Text("Play On Start", "开始时播放"), &c.PlayOnStart);
                ImGui::DragFloat(EditorLocale::Text("Speed", "速度"), &c.PlaybackSpeed,
                    0.01f, 0.0f, 8.0f);
                ImGui::DragFloat(EditorLocale::Text("Crossfade", "交叉淡化"),
                    &c.FadeDuration, 0.01f, 0.0f, 5.0f);

                const char* clipName = "(none)";
                if (c.CurrentClipIndex >= 0
                    && static_cast<size_t>(c.CurrentClipIndex) < animations.size())
                {
                    clipName = animations[c.CurrentClipIndex].Name.c_str();
                }
                ImGui::Separator();
                ImGui::Text(EditorLocale::Text("Playing: %s", "正在播放: %s"), clipName);
                ImGui::Text(EditorLocale::Text("Time: %.3fs  Bones: %d  Clips: %d",
                    "时间: %.3fs  骨骼: %d  片段: %d"),
                    c.Time, static_cast<int>(model->GetBones().size()),
                    static_cast<int>(animations.size()));
            });
    }

} // namespace Wheatear
