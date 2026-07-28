#include "wtpch.h"
#include "ScriptGlue.h"

#include "Wheatear/Scene/Scene.h"
#include "Wheatear/Scene/Entity.h"
#include "Wheatear/Scene/Components.h"
#include "Wheatear/Core/Input.h"
#include "Wheatear/Core/KeyCodes.h"
#include "Wheatear/Core/MouseButtonCodes.h"
#include "Wheatear/Audio/AudioEngine.h"
#include "ScriptEngine.h"

#include <box2d/b2_body.h>

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

namespace Wheatear {

#define WT_ADD_INTERNAL_CALL(Name) \
    mono_add_internal_call("Wheatear.InternalCalls::" #Name, (void*)Name)

    // =========================================================
    //  类型注册表
    //  用 std::unordered_map 代替一堆 if-else 字符串比较，
    //  新增组件只需在 RegisterFunctions() 末尾加一行 REGISTER 宏
    // =========================================================

    using HasComponentFn = bool(*)(Entity&);
    using AddComponentFn = void(*)(Entity&);

    static std::unordered_map<MonoType*, HasComponentFn> s_HasComponentFns;
    static std::unordered_map<MonoType*, AddComponentFn> s_AddComponentFns;

    // 注册辅助宏
#define REGISTER_COMPONENT(CppType, CSharpName)                                         \
    {                                                                                    \
        MonoType* mt = mono_reflection_type_from_name(                                  \
            (char*)(CSharpName), ScriptEngine::GetCoreAssemblyImage());                 \
        if (mt) {                                                                        \
            s_HasComponentFns[mt] = [](Entity& e) { return e.HasComponent<CppType>(); };\
            s_AddComponentFns[mt] = [](Entity& e) {                                     \
                if (!e.HasComponent<CppType>()) e.AddComponent<CppType>();              \
            };                                                                           \
        } else {                                                                         \
            WT_CORE_WARN("REGISTER_COMPONENT: type '{}' not found", CSharpName);        \
        }                                                                                \
    }

    static std::string MonoStringToString(MonoString* value)
    {
        if (!value)
            return {};

        char* cStr = mono_string_to_utf8(value);
        std::string result = cStr ? cStr : "";
        mono_free(cStr);
        return result;
    }
    // =========================================================
    //  Entity
    // =========================================================

    static MonoString* Entity_GetTag(uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity)
            return mono_string_new(mono_domain_get(), "");
        return mono_string_new(mono_domain_get(),
            entity.GetComponent<TagComponent>().Tag.c_str());
    }

    static void Entity_Destroy(uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;
        scene->DestroyEntity(entity);
    }

    static bool Entity_HasComponent(uint64_t entityID, MonoReflectionType* componentType)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return false;

        MonoType* monoType = mono_reflection_type_get_type(componentType);
        auto it = s_HasComponentFns.find(monoType);
        if (it != s_HasComponentFns.end())
            return it->second(entity);

        WT_CORE_WARN("Entity_HasComponent: unregistered type '{}'",
            mono_type_get_name(monoType));
        return false;
    }

    static void Entity_AddComponent(uint64_t entityID, MonoReflectionType* componentType)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        MonoType* monoType = mono_reflection_type_get_type(componentType);
        auto it = s_AddComponentFns.find(monoType);
        if (it != s_AddComponentFns.end())
        {
            it->second(entity);
            return;
        }

        WT_CORE_WARN("Entity_AddComponent: unregistered type '{}'",
            mono_type_get_name(monoType));
    }

    // 在场景中按名字查找 Entity，返回其 UUID（找不到返回 0）
    // C# 端：ulong id = InternalCalls.Scene_FindEntityByName("Player");
    static uint64_t Scene_FindEntityByName(MonoString* name)
    {
        char* cStr = mono_string_to_utf8(name);
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByName(cStr);
        mono_free(cStr);
        if (!entity) return 0;
        return (uint64_t)entity.GetUUID();
    }

    // =========================================================
    //  Transform
    // =========================================================

    static void TransformComponent_GetTranslation(uint64_t entityID, glm::vec3* outTranslation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        WT_CORE_ASSERT(entity, "Entity not found!");
        *outTranslation = entity.GetComponent<TransformComponent>().Translation;
    }

    static void TransformComponent_SetTranslation(uint64_t entityID, glm::vec3* translation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        entity.GetComponent<TransformComponent>().Translation = *translation;
    }

    static void TransformComponent_GetRotation(uint64_t entityID, glm::vec3* outRotation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        *outRotation = entity.GetComponent<TransformComponent>().Rotation;
    }

    static void TransformComponent_SetRotation(uint64_t entityID, glm::vec3* rotation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        entity.GetComponent<TransformComponent>().Rotation = *rotation;
    }

    static void TransformComponent_GetScale(uint64_t entityID, glm::vec3* outScale)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        *outScale = entity.GetComponent<TransformComponent>().Scale;
    }

    static void TransformComponent_SetScale(uint64_t entityID, glm::vec3* scale)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        entity.GetComponent<TransformComponent>().Scale = *scale;
    }

    // =========================================================
    //  Rigidbody2D
    // =========================================================

    static void Rigidbody2DComponent_ApplyLinearImpulse(
        uint64_t entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) return;
        body->ApplyLinearImpulse(
            b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
    }

    static void Rigidbody2DComponent_GetLinearVelocity(uint64_t entityID, glm::vec2* outVelocity)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
        {
            *outVelocity = {}; return;
        }

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) { *outVelocity = {}; return; }

        const b2Vec2& vel = body->GetLinearVelocity();
        *outVelocity = { vel.x, vel.y };
    }

    static void Rigidbody2DComponent_SetLinearVelocity(uint64_t entityID, glm::vec2* velocity)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) return;
        body->SetLinearVelocity(b2Vec2(velocity->x, velocity->y));
    }

    // GravityScale —— 读取 / 设置重力缩放比例（0 = 不受重力）
    static float Rigidbody2DComponent_GetGravityScale(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return 1.0f;

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) return entity.GetComponent<Rigidbody2DComponent>().GravityScale;
        return body->GetGravityScale();
    }

    static void Rigidbody2DComponent_SetGravityScale(uint64_t entityID, float scale)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        rb2d.GravityScale = scale;   // 同时写回组件数据，以便序列化保存

        b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (body) body->SetGravityScale(scale);
    }

    // FixedRotation —— 锁定旋转
    static bool Rigidbody2DComponent_GetFixedRotation(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return false;
        return entity.GetComponent<Rigidbody2DComponent>().FixedRotation;
    }

    static void Rigidbody2DComponent_SetFixedRotation(uint64_t entityID, bool fixed)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        rb2d.FixedRotation = fixed;

        b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (body) body->SetFixedRotation(fixed);
    }

    // BodyType
    static int Rigidbody2DComponent_GetBodyType(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return 0;
        return static_cast<int>(entity.GetComponent<Rigidbody2DComponent>().Type);
    }

    static void Rigidbody2DComponent_SetBodyType(uint64_t entityID, int type)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        rb2d.Type = static_cast<Rigidbody2DComponent::BodyType>(type);

        b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (body)
        {
            b2BodyType b2Type = b2_staticBody;
            switch (rb2d.Type)
            {
            case Rigidbody2DComponent::BodyType::Dynamic:   b2Type = b2_dynamicBody;   break;
            case Rigidbody2DComponent::BodyType::Kinematic: b2Type = b2_kinematicBody; break;
            default: break;
            }
            body->SetType(b2Type);
        }
    }

    // =========================================================
    //  Input
    // =========================================================

    static bool Input_IsKeyDown(int keycode)
    {
        return Input::IsKeyPressed(keycode);
    }


    static bool Input_IsMouseButtonDown(int button)
    {
        return Input::IsMouseButtonPressed(button);
    }

    static void Input_GetMousePosition(glm::vec2* outPosition)
    {
        auto [x, y] = Input::GetMousePosition();
        *outPosition = { x, y };
    }

    // =========================================================
    //  Time
    // =========================================================

    static float Time_GetDeltaTime()
    {
        return ScriptEngine::GetDeltaTime();
    }

    static float Time_GetElapsedTime()
    {
        return ScriptEngine::GetElapsedTime();
    }

    static uint64_t Time_GetFrameCount()
    {
        return ScriptEngine::GetFrameCount();
    }

    // =========================================================
    //  Log
    // =========================================================

    static void Log_Info(MonoString* message)
    {
        WT_CORE_INFO("[C#] {}", MonoStringToString(message));
    }

    static void Log_Warn(MonoString* message)
    {
        WT_CORE_WARN("[C#] {}", MonoStringToString(message));
    }

    static void Log_Error(MonoString* message)
    {
        WT_CORE_ERROR("[C#] {}", MonoStringToString(message));
    }

    // =========================================================
    //  SpriteRenderer
    // =========================================================
    static void SpriteRendererComponent_GetColor(uint64_t entityID, glm::vec4* outColor)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) { *outColor = glm::vec4(1.0f); return; }
        *outColor = entity.GetComponent<SpriteRendererComponent>().Color;
    }

    static void SpriteRendererComponent_SetColor(uint64_t entityID, glm::vec4* color)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return;
        entity.GetComponent<SpriteRendererComponent>().Color = *color;
    }

    static float SpriteRendererComponent_GetTilingFactor(uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return 1.0f;
        return entity.GetComponent<SpriteRendererComponent>().TilingFactor;
    }

    static void SpriteRendererComponent_SetTilingFactor(uint64_t entityID, float tilingFactor)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return;
        entity.GetComponent<SpriteRendererComponent>().TilingFactor = tilingFactor;
    }

    static bool SpriteRendererComponent_GetFlipX(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return false;
        return entity.GetComponent<SpriteRendererComponent>().FlipX;
    }

    static void SpriteRendererComponent_SetFlipX(UUID entityID, bool flipX)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return;
        entity.GetComponent<SpriteRendererComponent>().FlipX = flipX;
    }

    // =========================================================
    //  SpriteAnimator
    // =========================================================

    static void SpriteAnimatorComponent_Play(uint64_t entityID, MonoString* clipName)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return;
        char* cStr = mono_string_to_utf8(clipName);
        entity.GetComponent<SpriteAnimatorComponent>().Play(cStr);
        mono_free(cStr);
    }

    static void SpriteAnimatorComponent_Stop(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return;
        entity.GetComponent<SpriteAnimatorComponent>().IsPlaying = false;
    }

    static void SpriteAnimatorComponent_Resume(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return;
        entity.GetComponent<SpriteAnimatorComponent>().IsPlaying = true;
    }

    static MonoString* SpriteAnimatorComponent_GetCurrentClip(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>())
            return mono_string_new(mono_domain_get(), "");
        const auto& name = entity.GetComponent<SpriteAnimatorComponent>().CurrentClipName;
        return mono_string_new(mono_domain_get(), name.c_str());
    }

    static bool SpriteAnimatorComponent_IsFinished(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return false;
        return entity.GetComponent<SpriteAnimatorComponent>().IsFinished;
    }

    // =========================================================
    //  Audio
    // =========================================================

    // 最简单的一次性播放，C# 里 Audio.PlaySound("path") 就走这里
    static void Audio_PlaySound(MonoString* filepath, float volume)
    {
        char* path = mono_string_to_utf8(filepath);
        AudioEngine::PlaySound(std::string(path), volume);
        mono_free(path);
    }

    // 播放并返回句柄，C# 里可以拿到 handle 再控制
    static uint32_t Audio_PlaySoundWithHandle(MonoString* filepath,
        float volume, bool loop)
    {
        char* path = mono_string_to_utf8(filepath);
        uint32_t handle = AudioEngine::PlaySoundWithHandle(
            std::string(path), volume, loop);
        mono_free(path);
        return handle;
    }

    static void Audio_StopSound(uint32_t handle)
    {
        AudioEngine::StopSound(handle);
    }

    static void Audio_PauseSound(uint32_t handle)
    {
        AudioEngine::PauseSound(handle);
    }

    static void Audio_ResumeSound(uint32_t handle)
    {
        AudioEngine::ResumeSound(handle);
    }

    static void Audio_SetVolume(uint32_t handle, float volume)
    {
        AudioEngine::SetVolume(handle, volume);
    }

    static bool Audio_IsPlaying(uint32_t handle)
    {
        return AudioEngine::IsPlaying(handle);
    }

    // ---- AudioSourceComponent 组件读写 ----
    static void AudioSourceComponent_Play(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        if (asc.AudioFilePath.empty()) return;

        // 如果已经在播放，先停掉
        if (asc.RuntimeHandle != 0)
            AudioEngine::StopSound(asc.RuntimeHandle);

        asc.RuntimeHandle = AudioEngine::PlaySoundWithHandle(
            asc.AudioFilePath, asc.Volume, asc.Loop);
    }

    static void AudioSourceComponent_Stop(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        if (asc.RuntimeHandle != 0)
        {
            AudioEngine::StopSound(asc.RuntimeHandle);
            asc.RuntimeHandle = 0;
        }
    }

    static bool AudioSourceComponent_IsPlaying(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return false;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        return AudioEngine::IsPlaying(asc.RuntimeHandle);
    }

    static float AudioSourceComponent_GetVolume(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return 1.0f;
        return entity.GetComponent<AudioSourceComponent>().Volume;
    }

    static void AudioSourceComponent_SetVolume(UUID entityID, float volume)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        asc.Volume = volume;
        // 如果正在播放，实时更新音量
        if (asc.RuntimeHandle != 0)
            AudioEngine::SetVolume(asc.RuntimeHandle, volume);
    }

    // =========================================================
    //  UI
    // =========================================================
    // ── UI: UIWidgetComponent ──────────────────────────────────────────────────

    static bool UICanvasComponent_GetVisible(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UICanvasComponent>()) return false;
        return entity.GetComponent<UICanvasComponent>().Visible;
    }

    static void UICanvasComponent_SetVisible(uint64_t entityID, bool visible)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UICanvasComponent>()) return;
        entity.GetComponent<UICanvasComponent>().Visible = visible;
    }

    static bool UIWidgetComponent_GetVisible(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return false;
        return entity.GetComponent<UIWidgetComponent>().Visible;
    }

    static void UIWidgetComponent_SetVisible(uint64_t entityID, bool visible)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        entity.GetComponent<UIWidgetComponent>().Visible = visible;
    }

    static void UIWidgetComponent_GetPosition(uint64_t entityID, glm::vec2* outPos)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        *outPos = entity.GetComponent<UIWidgetComponent>().Position;
    }

    static void UIWidgetComponent_SetPosition(uint64_t entityID, glm::vec2* pos)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        entity.GetComponent<UIWidgetComponent>().Position = *pos;
    }

    static void UIWidgetComponent_GetSize(uint64_t entityID, glm::vec2* outSize)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        *outSize = entity.GetComponent<UIWidgetComponent>().Size;
    }

    static void UIWidgetComponent_SetSize(uint64_t entityID, glm::vec2* size)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        entity.GetComponent<UIWidgetComponent>().Size = *size;
    }

    // ── UI: UIImageComponent ───────────────────────────────────────────────────

    static void UIImageComponent_GetColor(uint64_t entityID, glm::vec4* outColor)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIImageComponent>()) return;
        *outColor = entity.GetComponent<UIImageComponent>().Color;
    }

    static void UIImageComponent_SetColor(uint64_t entityID, glm::vec4* color)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIImageComponent>()) return;
        entity.GetComponent<UIImageComponent>().Color = *color;
    }

    // ── UI: UIProgressBarComponent ────────────────────────────────────────────

    static float UIProgressBarComponent_GetValue(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return 0.0f;
        return entity.GetComponent<UIProgressBarComponent>().Value;
    }

    static void UIProgressBarComponent_SetValue(uint64_t entityID, float value)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        entity.GetComponent<UIProgressBarComponent>().Value = value;
    }

    static float UIProgressBarComponent_GetMaxValue(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return 1.0f;
        return entity.GetComponent<UIProgressBarComponent>().MaxValue;
    }

    static void UIProgressBarComponent_SetMaxValue(uint64_t entityID, float maxValue)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        entity.GetComponent<UIProgressBarComponent>().MaxValue = maxValue;
    }

    static void UIProgressBarComponent_GetForegroundColor(uint64_t entityID, glm::vec4* outColor)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        *outColor = entity.GetComponent<UIProgressBarComponent>().ForegroundColor;
    }

    static void UIProgressBarComponent_SetForegroundColor(uint64_t entityID, glm::vec4* color)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        entity.GetComponent<UIProgressBarComponent>().ForegroundColor = *color;
    }

    // ── UI: UIButtonComponent ─────────────────────────────────────────────────

    static bool UIButtonComponent_GetIsHovered(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return false;
        return entity.GetComponent<UIButtonComponent>().IsHovered;
    }

    static bool UIButtonComponent_GetIsPressed(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return false;
        return entity.GetComponent<UIButtonComponent>().IsPressed;
    }

    // Button点击事件的核心：C++检测到点击后，调用此函数触发C#回调
    // 这个函数由UIInputSystem调用，不是InternalCall

    static void UIButtonComponent_SetOnClickFunction(uint64_t entityID, MonoString* funcName)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return;
        char* cStr = mono_string_to_utf8(funcName);
        entity.GetComponent<UIButtonComponent>().OnClickFunction = cStr;
        mono_free(cStr);
    }

    static MonoString* UIButtonComponent_GetOnClickFunction(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return nullptr;
        auto& func = entity.GetComponent<UIButtonComponent>().OnClickFunction;
        return mono_string_new(mono_domain_get(), func.c_str());
    }

    // ── UI: UITextComponent ───────────────────────────────────────────────────

    static MonoString* UITextComponent_GetText(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return nullptr;
        auto& text = entity.GetComponent<UITextComponent>().Text;
        return mono_string_new(mono_domain_get(), text.c_str());
    }

    static void UITextComponent_SetText(uint64_t entityID, MonoString* text)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        char* cStr = mono_string_to_utf8(text);
        entity.GetComponent<UITextComponent>().Text = cStr;
        mono_free(cStr);
    }

    static void UITextComponent_GetColor(uint64_t entityID, glm::vec4* outColor)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        *outColor = entity.GetComponent<UITextComponent>().Color;
    }

    static void UITextComponent_SetColor(uint64_t entityID, glm::vec4* color)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        entity.GetComponent<UITextComponent>().Color = *color;
    }

    static float UITextComponent_GetFontSize(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return 0.0f;
        return entity.GetComponent<UITextComponent>().FontSize;
    }

    static void UITextComponent_SetFontSize(uint64_t entityID, float fontSize)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        entity.GetComponent<UITextComponent>().FontSize = std::max(1.0f, fontSize);
    }

    static MonoString* UITextComponent_GetFontPath(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return nullptr;
        auto& fontPath = entity.GetComponent<UITextComponent>().FontPath;
        return mono_string_new(mono_domain_get(), fontPath.c_str());
    }

    static void UITextComponent_SetFontPath(uint64_t entityID, MonoString* fontPath)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        char* cStr = mono_string_to_utf8(fontPath);
        entity.GetComponent<UITextComponent>().FontPath = cStr;
        mono_free(cStr);
    }

    // =========================================================
    //  Arcade Combat
    // =========================================================

    static bool ArcadeCombatLevelComponent_GetPaused(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<ArcadeCombatLevelComponent>()) return false;
        return entity.GetComponent<ArcadeCombatLevelComponent>().RuntimePaused;
    }

    static bool ArcadeCombatLevelComponent_GetBossIntroStarted(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<ArcadeCombatLevelComponent>()) return false;
        return entity.GetComponent<ArcadeCombatLevelComponent>().RuntimeBossIntroStarted;
    }

    static bool ArcadeCombatLevelComponent_GetBossIntroFinished(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<ArcadeCombatLevelComponent>()) return false;
        return entity.GetComponent<ArcadeCombatLevelComponent>().RuntimeBossIntroFinished;
    }

    static bool ArcadeCombatLevelComponent_GetVictory(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<ArcadeCombatLevelComponent>()) return false;
        return entity.GetComponent<ArcadeCombatLevelComponent>().RuntimeVictory;
    }

    static bool ArcadeCombatLevelComponent_GetDefeat(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<ArcadeCombatLevelComponent>()) return false;
        return entity.GetComponent<ArcadeCombatLevelComponent>().RuntimeDefeat;
    }

    static void ArcadeCombatLevelComponent_RequestSceneCommand(uint64_t entityID, MonoString* command)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<ArcadeCombatLevelComponent>()) return;

        char* cStr = mono_string_to_utf8(command);
        entity.GetComponent<ArcadeCombatLevelComponent>().RuntimeRequestedCommand = cStr;
        mono_free(cStr);
    }
    // =========================================================
    //  Scene
    // =========================================================

    static uint64_t Scene_InstantiateFromPrefab(MonoString* prefabPath, glm::vec3* position)
    {
        char* cStr = mono_string_to_utf8(prefabPath);
        std::filesystem::path path(cStr);
        mono_free(cStr);

        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->InstantiateFromPrefab(path, *position);
        if (!entity) return 0;
        return (uint64_t)entity.GetUUID();
    }

    // 跨脚本通信：通过 UUID 拿到另一个 Entity 的脚本实例（返回 object，C# 端强转）
    static MonoObject* Entity_GetScriptInstance(uint64_t entityID)
    {
        Ref<ScriptInstance> instance = ScriptEngine::GetEntityScriptInstance(entityID);
        if (!instance) return nullptr;
        return instance->GetMonoObject();
    }

    // =========================================================
    //  注册所有函数
    // =========================================================

    void ScriptGlue::RegisterFunctions()
    {
        s_HasComponentFns.clear();
        s_AddComponentFns.clear();

        // ── Entity ────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(Entity_GetTag);
        WT_ADD_INTERNAL_CALL(Entity_Destroy);
        WT_ADD_INTERNAL_CALL(Entity_HasComponent);
        WT_ADD_INTERNAL_CALL(Entity_AddComponent);
        WT_ADD_INTERNAL_CALL(Entity_GetScriptInstance);

        // ── Scene ─────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(Scene_FindEntityByName);
        WT_ADD_INTERNAL_CALL(Scene_InstantiateFromPrefab);

        // ── Transform ─────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
        WT_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
        WT_ADD_INTERNAL_CALL(TransformComponent_GetRotation);
        WT_ADD_INTERNAL_CALL(TransformComponent_SetRotation);
        WT_ADD_INTERNAL_CALL(TransformComponent_GetScale);
        WT_ADD_INTERNAL_CALL(TransformComponent_SetScale);

        // ── Rigidbody2D ───────────────────────────────────────
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulse);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetLinearVelocity);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetLinearVelocity);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetGravityScale);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetGravityScale);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetFixedRotation);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetFixedRotation);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetBodyType);
        WT_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetBodyType);

        // ── Input ─────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(Input_IsKeyDown);
        WT_ADD_INTERNAL_CALL(Input_IsMouseButtonDown);
        WT_ADD_INTERNAL_CALL(Input_GetMousePosition);

        // ── Time ──────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(Time_GetDeltaTime);
        WT_ADD_INTERNAL_CALL(Time_GetElapsedTime);
        WT_ADD_INTERNAL_CALL(Time_GetFrameCount);

        // ── Log ───────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(Log_Info);
        WT_ADD_INTERNAL_CALL(Log_Warn);
        WT_ADD_INTERNAL_CALL(Log_Error);

        // ── SpriteRenderer ────────────────────────────────────
        WT_ADD_INTERNAL_CALL(SpriteRendererComponent_GetColor);
        WT_ADD_INTERNAL_CALL(SpriteRendererComponent_SetColor);
        WT_ADD_INTERNAL_CALL(SpriteRendererComponent_GetTilingFactor);
        WT_ADD_INTERNAL_CALL(SpriteRendererComponent_SetTilingFactor);
        WT_ADD_INTERNAL_CALL(SpriteRendererComponent_GetFlipX);
        WT_ADD_INTERNAL_CALL(SpriteRendererComponent_SetFlipX);

        // ── SpriteAnimator ────────────────────────────────────
        WT_ADD_INTERNAL_CALL(SpriteAnimatorComponent_Play);
        WT_ADD_INTERNAL_CALL(SpriteAnimatorComponent_Stop);
        WT_ADD_INTERNAL_CALL(SpriteAnimatorComponent_Resume);
        WT_ADD_INTERNAL_CALL(SpriteAnimatorComponent_GetCurrentClip);
        WT_ADD_INTERNAL_CALL(SpriteAnimatorComponent_IsFinished);

        // ── Audio ─────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(Audio_PlaySound);
        WT_ADD_INTERNAL_CALL(Audio_PlaySoundWithHandle);
        WT_ADD_INTERNAL_CALL(Audio_StopSound);
        WT_ADD_INTERNAL_CALL(Audio_PauseSound);
        WT_ADD_INTERNAL_CALL(Audio_ResumeSound);
        WT_ADD_INTERNAL_CALL(Audio_SetVolume);
        WT_ADD_INTERNAL_CALL(Audio_IsPlaying);
        WT_ADD_INTERNAL_CALL(AudioSourceComponent_Play);
        WT_ADD_INTERNAL_CALL(AudioSourceComponent_Stop);
        WT_ADD_INTERNAL_CALL(AudioSourceComponent_IsPlaying);
        WT_ADD_INTERNAL_CALL(AudioSourceComponent_GetVolume);
        WT_ADD_INTERNAL_CALL(AudioSourceComponent_SetVolume);

        // ── UI Components ─────────────────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(UICanvasComponent_GetVisible);
        WT_ADD_INTERNAL_CALL(UICanvasComponent_SetVisible);
        WT_ADD_INTERNAL_CALL(UIWidgetComponent_GetVisible);
        WT_ADD_INTERNAL_CALL(UIWidgetComponent_SetVisible);
        WT_ADD_INTERNAL_CALL(UIWidgetComponent_GetPosition);
        WT_ADD_INTERNAL_CALL(UIWidgetComponent_SetPosition);
        WT_ADD_INTERNAL_CALL(UIWidgetComponent_GetSize);
        WT_ADD_INTERNAL_CALL(UIWidgetComponent_SetSize);

        WT_ADD_INTERNAL_CALL(UIImageComponent_GetColor);
        WT_ADD_INTERNAL_CALL(UIImageComponent_SetColor);

        WT_ADD_INTERNAL_CALL(UIProgressBarComponent_GetValue);
        WT_ADD_INTERNAL_CALL(UIProgressBarComponent_SetValue);
        WT_ADD_INTERNAL_CALL(UIProgressBarComponent_GetMaxValue);
        WT_ADD_INTERNAL_CALL(UIProgressBarComponent_SetMaxValue);
        WT_ADD_INTERNAL_CALL(UIProgressBarComponent_GetForegroundColor);
        WT_ADD_INTERNAL_CALL(UIProgressBarComponent_SetForegroundColor);

        WT_ADD_INTERNAL_CALL(UIButtonComponent_GetIsHovered);
        WT_ADD_INTERNAL_CALL(UIButtonComponent_GetIsPressed);
        WT_ADD_INTERNAL_CALL(UIButtonComponent_SetOnClickFunction);
        WT_ADD_INTERNAL_CALL(UIButtonComponent_GetOnClickFunction);

        WT_ADD_INTERNAL_CALL(UITextComponent_GetText);
        WT_ADD_INTERNAL_CALL(UITextComponent_SetText);
        WT_ADD_INTERNAL_CALL(UITextComponent_GetColor);
        WT_ADD_INTERNAL_CALL(UITextComponent_SetColor);
        WT_ADD_INTERNAL_CALL(UITextComponent_GetFontSize);
        WT_ADD_INTERNAL_CALL(UITextComponent_SetFontSize);
        WT_ADD_INTERNAL_CALL(UITextComponent_GetFontPath);
        WT_ADD_INTERNAL_CALL(UITextComponent_SetFontPath);

        // ── Arcade Combat ────────────────────────────────────────────────────────
        WT_ADD_INTERNAL_CALL(ArcadeCombatLevelComponent_GetPaused);
        WT_ADD_INTERNAL_CALL(ArcadeCombatLevelComponent_GetBossIntroStarted);
        WT_ADD_INTERNAL_CALL(ArcadeCombatLevelComponent_GetBossIntroFinished);
        WT_ADD_INTERNAL_CALL(ArcadeCombatLevelComponent_GetVictory);
        WT_ADD_INTERNAL_CALL(ArcadeCombatLevelComponent_GetDefeat);
        WT_ADD_INTERNAL_CALL(ArcadeCombatLevelComponent_RequestSceneCommand);
        // ── 类型注册表（HasComponent / AddComponent 用）──────
        // 新增组件只需在这里加一行，无需修改 HasComponent/AddComponent 函数体
        REGISTER_COMPONENT(TransformComponent, "Wheatear.TransformComponent");
        REGISTER_COMPONENT(Rigidbody2DComponent, "Wheatear.Rigidbody2DComponent");
        REGISTER_COMPONENT(SpriteAnimatorComponent, "Wheatear.SpriteAnimatorComponent");
        REGISTER_COMPONENT(SpriteRendererComponent, "Wheatear.SpriteRendererComponent");
        REGISTER_COMPONENT(CircleRendererComponent, "Wheatear.CircleRendererComponent");
        REGISTER_COMPONENT(BoxCollider2DComponent, "Wheatear.BoxCollider2DComponent");
        REGISTER_COMPONENT(CircleCollider2DComponent, "Wheatear.CircleCollider2DComponent");
        REGISTER_COMPONENT(CameraComponent, "Wheatear.CameraComponent");
        REGISTER_COMPONENT(UICanvasComponent, "Wheatear.UICanvasComponent");
        REGISTER_COMPONENT(UIWidgetComponent, "Wheatear.UIWidgetComponent");
        REGISTER_COMPONENT(UIImageComponent, "Wheatear.UIImageComponent");
        REGISTER_COMPONENT(UITextComponent, "Wheatear.UITextComponent");
        REGISTER_COMPONENT(UIButtonComponent, "Wheatear.UIButtonComponent");
        REGISTER_COMPONENT(UIProgressBarComponent, "Wheatear.UIProgressBarComponent");
        REGISTER_COMPONENT(AudioSourceComponent, "Wheatear.AudioSourceComponent");
        REGISTER_COMPONENT(ArcadeCombatLevelComponent, "Wheatear.ArcadeCombatLevelComponent");
    }

} // namespace Wheatear
