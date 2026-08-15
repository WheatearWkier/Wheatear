using System;

namespace Wheatear
{
    public static class Mathf
    {
        public const float PI = 3.14159265358979323846f;
        public const float Deg2Rad = PI / 180.0f;
        public const float Rad2Deg = 180.0f / PI;

        public static float Clamp(float value, float min, float max) => MathF.Min(MathF.Max(value, min), max);
        public static float Clamp01(float value) => Clamp(value, 0.0f, 1.0f);
        public static float Lerp(float a, float b, float t) => a + (b - a) * Clamp01(t);
        public static float MoveTowards(float current, float target, float maxDelta)
        {
            if (MathF.Abs(target - current) <= maxDelta)
                return target;
            return current + MathF.Sign(target - current) * maxDelta;
        }

        public static float Abs(float value) => MathF.Abs(value);
        public static float Sign(float value) => MathF.Sign(value);
        public static float Sqrt(float value) => MathF.Sqrt(value);
    }
}
