#pragma once
#include <vector>
#include <deque>
#include <string>
#include <format>

#pragma warning (disable : 26495)

class Vec2
{
public:
	float x = 0.f, y = 0.f;

public:
	void Zero()
	{
		x = y = 0.f;
	}

	Vec2(float X = 0.f, float Y = 0.f)
	{
		x = X; y = Y;
	}

	Vec2(float* v)
	{
		x = v[0]; y = v[1];
	}

	Vec2(const float* v)
	{
		x = v[0]; y = v[1];
	}

	Vec2(const Vec2& v)
	{
		x = v.x; y = v.y;
	}

	Vec2& operator=(const Vec2& v)
	{
		x = v.x; y = v.y; return *this;
	}

	float& operator[](int i)
	{
		return ((float*)this)[i];
	}

	float operator[](int i) const
	{
		return ((float*)this)[i];
	}

	Vec2& operator+=(const Vec2& v)
	{
		x += v.x; y += v.y; return *this;
	}

	Vec2& operator-=(const Vec2& v)
	{
		x -= v.x; y -= v.y; return *this;
	}

	Vec2& operator*=(const Vec2& v)
	{
		x *= v.x; y *= v.y; return *this;
	}

	Vec2& operator/=(const Vec2& v)
	{
		x /= v.x; y /= v.y; return *this;
	}

	Vec2& operator+=(float v)
	{
		x += v; y += v; return *this;
	}

	Vec2& operator-=(float v)
	{
		x -= v; y -= v; return *this;
	}

	Vec2& operator*=(float v)
	{
		x *= v; y *= v; return *this;
	}

	Vec2& operator/=(float v)
	{
		x /= v; y /= v; return *this;
	}

	Vec2 operator+(const Vec2& v) const
	{
		return Vec2(x + v.x, y + v.y);
	}

	Vec2 operator-(const Vec2& v) const
	{
		return Vec2(x - v.x, y - v.y);
	}

	Vec2 operator*(const Vec2& v) const
	{
		return Vec2(x * v.x, y * v.y);
	}

	Vec2 operator/(const Vec2& v) const
	{
		return Vec2(x / v.x, y / v.y);
	}

	Vec2 operator+(float v) const
	{
		return Vec2(x + v, y + v);
	}

	Vec2 operator-(float v) const
	{
		return Vec2(x - v, y - v);
	}

	Vec2 operator*(float v) const
	{
		return Vec2(x * v, y * v);
	}

	Vec2 operator/(float v) const
	{
		return Vec2(x / v, y / v);
	}

	void Set(float X = 0.f, float Y = 0.f)
	{
		x = X; y = Y;
	}

	Vec2 Min(const Vec2& v) const
	{
		return Vec2(std::min<float>(x, v.x), std::min<float>(y, v.y));
	}

	Vec2 Max(const Vec2& v) const
	{
		return Vec2(std::max<float>(x, v.x), std::max<float>(y, v.y));
	}

	Vec2 Clamp(const Vec2& v1, const Vec2& v2) const
	{
		return Max(v1).Min(v2);
	}

	Vec2 Min(float v) const
	{
		return Vec2(std::min<float>(x, v), std::min<float>(y, v));
	}

	Vec2 Max(float v) const
	{
		return Vec2(std::max<float>(x, v), std::max<float>(y, v));
	}

	Vec2 Clamp(float v1, float v2) const
	{
		return Max(v1).Min(v2);
	}
	
	Vec2 Lerp(const Vec2& v, float t) const
	{
		return { x + (v.x - x) * t, y + (v.y - y) * t };
	}

	Vec2 Lerp(float v, float t) const
	{
		return { x + (v - x) * t, y + (v - y) * t };
	}

	Vec2 DeltaAngle(const Vec2& v) const
	{
		auto deltaAngle = [](const float flAngleA, const float flAngleB)
			{
				float flOut = fmodf((flAngleA - flAngleB) + 180.f, 360.f);
				return flOut += flOut < 0 ? 180.f : -180.f;
			};

		return { deltaAngle(x, v.x), deltaAngle(y, v.y) };
	}

	Vec2 DeltaAngle(float v) const
	{
		auto deltaAngle = [](const float flAngleA, const float flAngleB)
			{
				float flOut = fmodf((flAngleA - flAngleB) + 180.f, 360.f);
				return flOut += flOut < 0 ? 180.f : -180.f;
			};

		return { deltaAngle(x, v), deltaAngle(y, v) };
	}

	Vec2 LerpAngle(const Vec2& v, float t) const
	{
		auto shortDist = [](const float flAngleA, const float flAngleB)
			{
				const float flDelta = fmodf((flAngleA - flAngleB), 360.f);
				return fmodf(2 * flDelta, 360.f) - flDelta;
			};
		return { x - shortDist(x, v.x) * t, y - shortDist(y, v.y) * t };
	}

	Vec2 LerpAngle(float v, float t) const
	{
		auto shortDist = [](const float flAngleA, const float flAngleB)
			{
				const float flDelta = fmodf((flAngleA - flAngleB), 360.f);
				return fmodf(2 * flDelta, 360.f) - flDelta;
			};
		return { x - shortDist(x, v) * t, y - shortDist(y, v) * t };
	}

	float Length(void) const
	{
		return sqrtf(x * x + y * y);
	}

	float LengthSqr(void) const
	{
		return (x * x + y * y);
	}

	float DistTo(const Vec2& v) const
	{
		return (*this - v).Length();
	}

	float DistToSqr(const Vec2& v) const
	{
		return (*this - v).LengthSqr();
	}

	float Dot(const Vec2& v) const
	{
		return x * v.x + y * v.y;
	}

	bool IsZero(const float tolerance = 0.001f) const
	{
		return fabsf(x) < tolerance &&
			   fabsf(y) < tolerance;
	}
};
using Vector2D = Vec2;

class Vec3
{
public:
	float x = 0.f, y = 0.f, z = 0.f;

public:
	void Zero()
	{
		x = y = z = 0.f;
	}

	Vec3(float X = 0.f, float Y = 0.f, float Z = 0.f)
	{
		x = X; y = Y; z = Z;
	}

	Vec3(float* v)
	{
		x = v[0]; y = v[1]; z = v[2];
	}

	Vec3(const float* v)
	{
		x = v[0]; y = v[1]; z = v[2];
	}

	Vec3(const Vec3& v)
	{
		x = v.x; y = v.y; z = v.z;
	}

	Vec3(const Vec2& v)
	{
		x = v.x; y = v.y; z = 0.f;
	}

	Vec3& operator=(const Vec3& v)
	{
		x = v.x; y = v.y; z = v.z; return *this;
	}

	Vec3& operator-()
	{
		x = -x; y = -y; z = -z; return *this;
	}

	float& operator[](int i)
	{
		return ((float*)this)[i];
	}

	float operator[](int i) const
	{
		return ((float*)this)[i];
	}

	Vec3& operator+=(const Vec3& v)
	{
		x += v.x; y += v.y; z += v.z; return *this;
	}

	Vec3& operator-=(const Vec3& v)
	{
		x -= v.x; y -= v.y; z -= v.z; return *this;
	}

	Vec3& operator*=(const Vec3& v)
	{
		x *= v.x; y *= v.y; z *= v.z; return *this;
	}

	Vec3& operator/=(const Vec3& v)
	{
		x /= v.x; y /= v.y; z /= v.z; return *this;
	}

	Vec3& operator+=(float v)
	{
		x += v; y += v; z += v; return *this;
	}

	Vec3& operator-=(float v)
	{
		x -= v; y -= v; z -= v; return *this;
	}

	Vec3& operator*=(float v)
	{
		x *= v; y *= v; z *= v; return *this;
	}

	Vec3& operator/=(float v)
	{
		x /= v; y /= v; z /= v; return *this;
	}

	Vec3 operator+(const Vec3& v) const
	{
		return Vec3(x + v.x, y + v.y, z + v.z);
	}

	Vec3 operator-(const Vec3& v) const
	{
		return Vec3(x - v.x, y - v.y, z - v.z);
	}

	Vec3 operator*(const Vec3& v) const
	{
		return Vec3(x * v.x, y * v.y, z * v.z);
	}

	Vec3 operator/(const Vec3& v) const
	{
		return Vec3(x / v.x, y / v.y, z / v.z);
	}

	Vec3 operator+(float v) const
	{
		return Vec3(x + v, y + v, z + v);
	}

	Vec3 operator-(float v) const
	{
		return Vec3(x - v, y - v, z - v);
	}

	Vec3 operator*(float v) const
	{
		return Vec3(x * v, y * v, z * v);
	}

	Vec3 operator/(float v) const
	{
		return Vec3(x / v, y / v, z / v);
	}

	bool operator==(const Vec3& v) const
	{
		return (x == v.x && y == v.y && z == v.z);
	}

	bool operator!=(const Vec3& v) const
	{
		return (x != v.x || y != v.y || z != v.z);
	}

	void Set(float X = 0.f, float Y = 0.f, float Z = 0.f)
	{
		x = X; y = Y; z = Z;
	}

	Vec3 Min(const Vec3& v) const
	{
		return Vec3(std::min<float>(x, v.x), std::min<float>(y, v.y), std::min<float>(z, v.z));
	}

	Vec3 Max(const Vec3& v) const
	{
		return Vec3(std::max<float>(x, v.x), std::max<float>(y, v.y), std::max<float>(z, v.z));
	}

	Vec3 Clamp(const Vec3& v1, const Vec3& v2) const
	{
		return Max(v1).Min(v2);
	}

	Vec3 Min(float v) const
	{
		return Vec3(std::min<float>(x, v), std::min<float>(y, v), std::min<float>(z, v));
	}

	Vec3 Max(float v) const
	{
		return Vec3(std::max<float>(x, v), std::max<float>(y, v), std::max<float>(z, v));
	}

	Vec3 Clamp(float v1, float v2) const
	{
		return Max(v1).Min(v2);
	}

	Vec3 Lerp(const Vec3& v, float t) const
	{
		return { x + (v.x - x) * t, y + (v.y - y) * t, z + (v.z - z) * t };
	}

	Vec3 Lerp(float v, float t) const
	{
		return { x + (v - x) * t, y + (v - y) * t, z + (v - z) * t };
	}

	Vec3 DeltaAngle(const Vec3& v) const
	{
		auto deltaAngle = [](const float flAngleA, const float flAngleB)
			{
				float flOut = fmodf((flAngleA - flAngleB) + 180.f, 360.f);
				return flOut += flOut < 0 ? 180.f : -180.f;
			};

		return { deltaAngle(x, v.x), deltaAngle(y, v.y), deltaAngle(z, v.z) };
	}

	Vec3 DeltaAngle(float v) const
	{
		auto deltaAngle = [](const float flAngleA, const float flAngleB)
			{
				float flOut = fmodf((flAngleA - flAngleB) + 180.f, 360.f);
				return flOut += flOut < 0 ? 180.f : -180.f;
			};

		return { deltaAngle(x, v), deltaAngle(y, v), deltaAngle(z, v) };
	}

	Vec3 LerpAngle(const Vec3& v, float t) const
	{
		auto shortDist = [](const float flAngleA, const float flAngleB)
			{
				const float flDelta = fmodf((flAngleA - flAngleB), 360.f);
				return fmodf(2 * flDelta, 360.f) - flDelta;
			};
		return { x - shortDist(x, v.x) * t, y - shortDist(y, v.y) * t, z - shortDist(z, v.z) * t };
	}

	Vec3 LerpAngle(float v, float t) const
	{
		auto shortDist = [](const float flAngleA, const float flAngleB)
			{
				const float flDelta = fmodf((flAngleA - flAngleB), 360.f);
				return fmodf(2 * flDelta, 360.f) - flDelta;
			};
		return { x - shortDist(x, v) * t, y - shortDist(y, v) * t, z - shortDist(z, v) * t };
	}

	float Length(void) const
	{
		return sqrtf(x * x + y * y + z * z);
	}

	float LengthSqr(void) const
	{
		return (x * x + y * y + z * z);
	}

	float Normalize()
	{
		float flLength = Length();
		float flLengthNormal = 1.f / (FLT_EPSILON + flLength);

		x *= flLengthNormal;
		y *= flLengthNormal;
		z *= flLengthNormal;

		return flLength;
	}

	Vec3 Normalized()
	{
		float flLengthNormal = 1.f / (FLT_EPSILON + Length());
		return Vec3(x * flLengthNormal, y * flLengthNormal, z * flLengthNormal);
	}

	Vec3 Get2D()
	{
		return Vec3(x, y, 0);
	}

	float Length2D(void) const
	{
		return sqrtf(x * x + y * y);
	}

	float Length2DSqr(void) const
	{
		return (x * x + y * y);
	}

	float DistTo(const Vec3& v) const
	{
		return (*this - v).Length();
	}

	float DistToSqr(const Vec3& v) const
	{
		return (*this - v).LengthSqr();
	}

	float Dot(const Vec3& v) const
	{
		return x * v.x + y * v.y + z * v.z;
	}

	Vec3 Cross(const Vec3& v) const
	{
		return Vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
	}

	bool IsZero(const float tolerance = 0.001f) const
	{
		return fabsf(x) < tolerance &&
			   fabsf(y) < tolerance &&
			   fabsf(z) < tolerance;
	}

	Vec3 Scale(float fl)
	{
		return Vec3(x * fl, y * fl, z * fl);
	}

	void Init(float ix, float iy, float iz)
	{
		x = ix; y = iy; z = iz;
	}

	Vec3 toAngle() const noexcept
	{
		return { (atan2(-z, hypot(x, y))) * (180.f / 3.14159265358979323846f),
				 (atan2(y, x)) * (180.f / 3.14159265358979323846f),
				 0.f };
	}
	Vec3 fromAngle() const noexcept
	{
		return { cos(x * (3.14159265358979323846f / 180.f)) * cos(y * (3.14159265358979323846f / 180.f)),
				 cos(x * (3.14159265358979323846f / 180.f)) * sin(y * (3.14159265358979323846f / 180.f)),
				 -sin(x * (3.14159265358979323846f / 180.f)) };
	}
};
using Vector = Vec3;
using QAngle = Vec3;
using RadianEuler = Vec3;

class Vector4D
{
public:
	float x, y, z, w;
};
using Quaternion = Vector4D;

using matrix3x4 = float[3][4];
class VMatrix
{
public:
	Vector m[4][4];

public:
	inline const matrix3x4& As3x4() const
	{
		return *((const matrix3x4*)this);
	}
};

struct IntRange_t
{
	int Min = 0, Max = 0;

	bool operator==(IntRange_t other) const
	{
		return Min == other.Min && Max == other.Max;
	}

	bool operator!=(IntRange_t other) const
	{
		return Min != other.Min || Max != other.Max;
	}
};

struct FloatRange_t
{
	float Min = 0, Max = 0;

	bool operator==(FloatRange_t other) const
	{
		return Min == other.Min && Max == other.Max;
	}

	bool operator!=(FloatRange_t other) const
	{
		return Min != other.Min || Max != other.Max;
	}
};

using byte = unsigned char;
struct Color_t
{
	byte r = 255, g = 255, b = 255, a = 255;

	bool operator==(Color_t other) const
	{
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}

	bool operator!=(Color_t other) const
	{
		return r != other.r || g != other.g || b != other.b || a != other.a;
	}

	std::string ToHex() const
	{
		return std::format("\x7{:02X}{:02X}{:02X}", r, g, b);
	}

	std::string ToHexA() const
	{
		return std::format("\x8{:02X}{:02X}{:02X}{:02X}", r, g, b, a);
	}

	Color_t Lerp(Color_t to, float t, bool bAlpha = true) const
	{
		//a + (b - a) * t
		return {
			byte(r + (to.r - r) * t),
			byte(g + (to.g - g) * t),
			byte(b + (to.b - b) * t),
			byte(bAlpha ? a + (to.a - a) * t : a),
		};
	}
};

struct Gradient_t
{
	Color_t StartColor = {};
	Color_t EndColor = {};

	bool operator==(Gradient_t other) const
	{
		return StartColor == other.StartColor && EndColor == other.EndColor;
	}

	bool operator!=(Gradient_t other) const
	{
		return StartColor != other.StartColor || EndColor != other.EndColor;
	}
};

struct Chams_t
{
	std::vector<std::pair<std::string, Color_t>> Visible = { { "Original", Color_t() } };
	std::vector<std::pair<std::string, Color_t>> Occluded = {};
};

struct Glow_t
{
	int		Stencil = 0;
	int		Blur = 0;

	bool operator==(const Glow_t& other) const
	{
		return Stencil == other.Stencil && Blur == other.Blur;
	}
};

struct DragBox_t
{
	int x = 100;
	int y = 100;

	bool operator==(DragBox_t other) const
	{
		return x == other.x && y == other.y;
	}

	bool operator!=(DragBox_t other) const
	{
		return x != other.x || y != other.y;
	}
};

struct WindowBox_t
{
	int x = 100;
	int y = 100;
	int w = 200;
	int h = 150;

	bool operator==(WindowBox_t other) const
	{
		return x == other.x && y == other.y && w == other.w && h == other.h;
	}

	bool operator!=(WindowBox_t other) const
	{
		return x != other.x || y != other.y || w != other.w || h != other.h;
	}
};