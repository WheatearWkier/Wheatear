namespace Wheatear
{
    public struct Vector4
    {
        public float X, Y, Z, W;

        public Vector4(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public static Vector4 Zero => new Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        public static Vector4 One => new Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        public static Vector4 White => One;
        public static Vector4 Black => new Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        public static Vector4 operator +(Vector4 a, Vector4 b) => new Vector4(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W);
        public static Vector4 operator -(Vector4 a, Vector4 b) => new Vector4(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W);
        public static Vector4 operator *(Vector4 value, float scalar) => new Vector4(value.X * scalar, value.Y * scalar, value.Z * scalar, value.W * scalar);
        public static Vector4 operator *(float scalar, Vector4 value) => value * scalar;

        public override string ToString() => $"({X}, {Y}, {Z}, {W})";
    }
}
