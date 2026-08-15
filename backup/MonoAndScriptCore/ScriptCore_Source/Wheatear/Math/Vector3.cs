using System;

namespace Wheatear
{
    public struct Vector3
    {
        public float X, Y, Z;

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public static Vector3 Zero => new Vector3(0.0f, 0.0f, 0.0f);
        public static Vector3 One => new Vector3(1.0f, 1.0f, 1.0f);
        public static Vector3 Up => new Vector3(0.0f, 1.0f, 0.0f);
        public static Vector3 Right => new Vector3(1.0f, 0.0f, 0.0f);
        public static Vector3 Forward => new Vector3(0.0f, 0.0f, 1.0f);

        public float Length => MathF.Sqrt(X * X + Y * Y + Z * Z);
        public Vector3 Normalized => Length > 0.0001f ? this / Length : Zero;

        public static Vector3 operator +(Vector3 a, Vector3 b) => new Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        public static Vector3 operator -(Vector3 a, Vector3 b) => new Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        public static Vector3 operator -(Vector3 value) => new Vector3(-value.X, -value.Y, -value.Z);
        public static Vector3 operator *(Vector3 value, float scalar) => new Vector3(value.X * scalar, value.Y * scalar, value.Z * scalar);
        public static Vector3 operator *(float scalar, Vector3 value) => value * scalar;
        public static Vector3 operator /(Vector3 value, float scalar) => new Vector3(value.X / scalar, value.Y / scalar, value.Z / scalar);

        public static float Dot(Vector3 a, Vector3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        public static Vector3 Lerp(Vector3 a, Vector3 b, float t) => a + (b - a) * Mathf.Clamp01(t);

        public override string ToString() => $"({X}, {Y}, {Z})";
    }
}
