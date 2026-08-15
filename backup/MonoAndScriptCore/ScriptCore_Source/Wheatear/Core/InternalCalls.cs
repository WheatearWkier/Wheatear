using System;
using System.Runtime.CompilerServices;

namespace Wheatear
{
    internal static class InternalCalls
    {
        // ── Entity ────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string Entity_GetTag(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Entity_Destroy(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Entity_HasComponent(ulong entityID, Type componentType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Entity_AddComponent(ulong entityID, Type componentType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static object Entity_GetScriptInstance(ulong entityID);

        // ── Scene ─────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Scene_FindEntityByName(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Scene_InstantiateFromPrefab(string prefabPath, ref Vector3 position);

        // ── Transform ─────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_GetTranslation(ulong entityID, out Vector3 translation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_SetTranslation(ulong entityID, ref Vector3 translation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_GetRotation(ulong entityID, out Vector3 rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_SetRotation(ulong entityID, ref Vector3 rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_GetScale(ulong entityID, out Vector3 scale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void TransformComponent_SetScale(ulong entityID, ref Vector3 scale);

        // ── Rigidbody2D ───────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, ref Vector2 impulse, ref Vector2 point, bool wake);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_GetLinearVelocity(ulong entityID, out Vector2 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_SetLinearVelocity(ulong entityID, ref Vector2 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Rigidbody2DComponent_GetGravityScale(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_SetGravityScale(ulong entityID, float scale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Rigidbody2DComponent_GetFixedRotation(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_SetFixedRotation(ulong entityID, bool fixedRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static int Rigidbody2DComponent_GetBodyType(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Rigidbody2DComponent_SetBodyType(ulong entityID, int type);

        // ── Input ─────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Input_IsKeyDown(int keycode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Input_IsMouseButtonDown(int button);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Input_GetMousePosition(out Vector2 position);

        // ── Time ──────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Time_GetDeltaTime();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float Time_GetElapsedTime();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static ulong Time_GetFrameCount();

        // ── Log ───────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Log_Info(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Log_Warn(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Log_Error(string message);

        // ── SpriteRenderer ────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void SpriteRendererComponent_GetColor(ulong entityID, out Vector4 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void SpriteRendererComponent_SetColor(ulong entityID, ref Vector4 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float SpriteRendererComponent_GetTilingFactor(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void SpriteRendererComponent_SetTilingFactor(ulong entityID, float tilingFactor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool SpriteRendererComponent_GetFlipX(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void SpriteRendererComponent_SetFlipX(ulong entityID, bool flipX);

        // ── SpriteAnimator ────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void SpriteAnimatorComponent_Play(ulong entityID, string clipName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void SpriteAnimatorComponent_Stop(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void SpriteAnimatorComponent_Resume(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string SpriteAnimatorComponent_GetCurrentClip(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool SpriteAnimatorComponent_IsFinished(ulong entityID);

        // ── UIWidgetComponent ──────────────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool UICanvasComponent_GetVisible(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UICanvasComponent_SetVisible(ulong entityID, bool visible);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool UIWidgetComponent_GetVisible(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIWidgetComponent_SetVisible(ulong entityID, bool visible);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIWidgetComponent_GetPosition(ulong entityID, out Vector2 outPos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIWidgetComponent_SetPosition(ulong entityID, ref Vector2 pos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIWidgetComponent_GetSize(ulong entityID, out Vector2 outSize);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIWidgetComponent_SetSize(ulong entityID, ref Vector2 size);


        // ── UIImageComponent ───────────────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIImageComponent_GetColor(ulong entityID, out Vector4 outColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIImageComponent_SetColor(ulong entityID, ref Vector4 color);

        // ── UIProgressBarComponent ────────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float UIProgressBarComponent_GetValue(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIProgressBarComponent_SetValue(ulong entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float UIProgressBarComponent_GetMaxValue(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIProgressBarComponent_SetMaxValue(ulong entityID, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIProgressBarComponent_GetForegroundColor(ulong entityID, out Vector4 outColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIProgressBarComponent_SetForegroundColor(ulong entityID, ref Vector4 color);


        // ── UIButtonComponent ─────────────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool UIButtonComponent_GetIsHovered(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool UIButtonComponent_GetIsPressed(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UIButtonComponent_SetOnClickFunction(ulong entityID, string funcName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string UIButtonComponent_GetOnClickFunction(ulong entityID);


        // ── UITextComponent ───────────────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string UITextComponent_GetText(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UITextComponent_SetText(ulong entityID, string text);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UITextComponent_GetColor(ulong entityID, out Vector4 outColor);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UITextComponent_SetColor(ulong entityID, ref Vector4 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float UITextComponent_GetFontSize(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UITextComponent_SetFontSize(ulong entityID, float fontSize);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static string UITextComponent_GetFontPath(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void UITextComponent_SetFontPath(ulong entityID, string fontPath);

        // ── Arcade Combat ─────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool ArcadeCombatLevelComponent_GetPaused(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool ArcadeCombatLevelComponent_GetBossIntroStarted(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool ArcadeCombatLevelComponent_GetBossIntroFinished(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool ArcadeCombatLevelComponent_GetVictory(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool ArcadeCombatLevelComponent_GetDefeat(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void ArcadeCombatLevelComponent_RequestSceneCommand(ulong entityID, string command);

        // ── Audio ─────────────────────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Audio_PlaySound(string filepath, float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static uint Audio_PlaySoundWithHandle(string filepath,
                                                               float volume, bool loop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Audio_StopSound(uint handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Audio_PauseSound(uint handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Audio_ResumeSound(uint handle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void Audio_SetVolume(uint handle, float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool Audio_IsPlaying(uint handle);

        // ── AudioSourceComponent ──────────────────────────────
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void AudioSourceComponent_Play(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void AudioSourceComponent_Stop(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool AudioSourceComponent_IsPlaying(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float AudioSourceComponent_GetVolume(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void AudioSourceComponent_SetVolume(ulong entityID, float volume);
    }
}
