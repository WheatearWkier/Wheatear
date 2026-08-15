using System;

namespace Wheatear
{
    public struct Vector2
    {
        public float X, Y;

        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }

        public static Vector2 Zero => new Vector2(0.0f, 0.0f);
        public static Vector2 One => new Vector2(1.0f, 1.0f);

        public float Length => MathF.Sqrt(X * X + Y * Y);
        public Vector2 Normalized => Length > 0.0001f ? this / Length : Zero;

        public static Vector2 operator +(Vector2 a, Vector2 b) => new Vector2(a.X + b.X, a.Y + b.Y);
        public static Vector2 operator -(Vector2 a, Vector2 b) => new Vector2(a.X - b.X, a.Y - b.Y);
        public static Vector2 operator -(Vector2 value) => new Vector2(-value.X, -value.Y);
        public static Vector2 operator *(Vector2 value, float scalar) => new Vector2(value.X * scalar, value.Y * scalar);
        public static Vector2 operator *(float scalar, Vector2 value) => value * scalar;
        public static Vector2 operator /(Vector2 value, float scalar) => new Vector2(value.X / scalar, value.Y / scalar);

        public static float Dot(Vector2 a, Vector2 b) => a.X * b.X + a.Y * b.Y;
        public static Vector2 Lerp(Vector2 a, Vector2 b, float t) => a + (b - a) * Mathf.Clamp01(t);

        public override string ToString() => $"({X}, {Y})";
    }
}
