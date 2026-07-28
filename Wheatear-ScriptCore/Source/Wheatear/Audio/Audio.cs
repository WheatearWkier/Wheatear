namespace Wheatear
{
    // ── 静态工具类，随时随地播放音效，不需要组件 ──────────────
    public static class Audio
    {
        /// <summary>
        /// 一次性播放音效，最简单的用法
        /// Audio.PlaySound("assets/sounds/shoot.wav");
        /// </summary>
        public static void PlaySound(string filepath, float volume = 1.0f)
        {
            InternalCalls.Audio_PlaySound(filepath, volume);
        }

        /// <summary>
        /// 播放并返回句柄，可以后续控制这个音效
        /// uint handle = Audio.Play("assets/sounds/bgm.wav", 0.8f, loop: true);
        /// </summary>
        public static uint Play(string filepath, float volume = 1.0f, bool loop = false)
        {
            return InternalCalls.Audio_PlaySoundWithHandle(filepath, volume, loop);
        }

        public static void Stop(uint handle) => InternalCalls.Audio_StopSound(handle);
        public static void Pause(uint handle) => InternalCalls.Audio_PauseSound(handle);
        public static void Resume(uint handle) => InternalCalls.Audio_ResumeSound(handle);
        public static void SetVolume(uint handle, float volume)
            => InternalCalls.Audio_SetVolume(handle, volume);
        public static bool IsPlaying(uint handle)
            => InternalCalls.Audio_IsPlaying(handle);
    }
}