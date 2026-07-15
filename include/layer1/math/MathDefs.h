#pragma once

#include <cmath>
#include <limits>

enum class Intersection
{
    OUTSIDE = 0,
    INTERSECTS,
    INSIDE
};

class MathDefs
{
public:
    inline static float FloatInfinity();

    inline static bool CheckEqualFloats(float num1, float num2, float epsilon = 1e-6f);

    inline static bool CheckEqualDoubles(double num1, double num2, double epsilon = 1e-12);

    template <typename T1, typename T2>
    inline static bool CheckEqualNumber(T1 num1, T2 num2, double epsilon = 1e-6f);

    template <typename T>
    inline static bool CheckEqual3DPoints(T x1, T y1, T z1, T x2, T y2, T z2);

    template <typename T>
    inline static bool CheckEqual3DPoints(const T& pt1, const T& pt2);

    template <typename T>
    inline static T Epsilon();

    template <typename T>
    inline static T Clamp(T value, T min, T max);

    template <typename T, typename U>
    inline static T Min(T lhs, U rhs) { return lhs < rhs ? lhs : rhs; }

    template <typename T, typename U>
    inline static T Max(T lhs, U rhs) { return lhs > rhs ? lhs : rhs; }

    template <typename T>
    inline static T MaxValue();

    template <typename... Ts>
    inline static bool CheckValidNumber(Ts... values);

    template <typename T>
    inline static bool CheckValidNumbers(const T* data, size_t count);
};

float MathDefs::FloatInfinity()
{
    static constexpr float infinity = std::numeric_limits<float>::infinity();
    return infinity;
}

template <typename T>
T MathDefs::Epsilon()
{
    return std::numeric_limits<T>::epsilon();
}

template <typename T>
T MathDefs::Clamp(T value, T min, T max)
{
    if (value < min) {
        return min;
    } else if (value > max) {
        return max;
    } else {
        return value;
    }
}

template <typename T>
T MathDefs::MaxValue()
{
    return (std::numeric_limits<T>::max)();
}

inline bool MathDefs::CheckEqualFloats(float num1, float num2, float epsilon)
{
    return std::fabs(num1 - num2) < epsilon;
}

inline bool MathDefs::CheckEqualDoubles(double num1, double num2, double epsilon)
{
    return std::fabs(num1 - num2) < epsilon;
}

template <typename T1, typename T2>
inline bool MathDefs::CheckEqualNumber(T1 num1, T2 num2, double epsilon)
{
    static_assert(std::is_same_v<T1, float> || std::is_same_v<T1, double>);
    static_assert(std::is_same_v<T2, float> || std::is_same_v<T2, double>);

    return std::abs(num1 - num2) < epsilon;
}

template <typename T>
inline bool MathDefs::CheckEqual3DPoints(T x1, T y1, T z1, T x2, T y2, T z2)
{
    return x1 == x2 && y1 == y2 && z1 == z2;
}

template <>
inline bool MathDefs::CheckEqual3DPoints<float>(float x1, float y1, float z1, float x2, float y2, float z2)
{
    return CheckEqualFloats(x1, x2) && CheckEqualFloats(y1, y2) && CheckEqualFloats(z1, z2);
}

template <>
inline bool MathDefs::CheckEqual3DPoints<double>(double x1, double y1, double z1, double x2, double y2, double z2)
{
    return CheckEqualDoubles(x1, x2) && CheckEqualDoubles(y1, y2) && CheckEqualDoubles(z1, z2);
}

template <typename T>
inline bool MathDefs::CheckEqual3DPoints(const T& pt1, const T& pt2)
{
    return CheckEqual3DPoints(pt1[0], pt1[1], pt1[2], pt2[0], pt2[1], pt2[2]);
}

template <typename... Ts>
inline bool MathDefs::CheckValidNumber(Ts... values)
{
    // static assert：only float or double type is allowed
    static_assert(((std::is_same_v<Ts, float> || std::is_same_v<Ts, double>) && ...),
        "All arguments must be float or double");

    // check not NaN and not INF
    return (... && (!std::isnan(values) && !std::isinf(values)));
}

template <typename T>
inline bool MathDefs::CheckValidNumbers(const T* data, size_t count)
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "Only float/double are supported");

    if (data == nullptr || count == 0) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        if (std::isnan(data[i]) || std::isinf(data[i])) {
            return false;
        }
    }
    return true;
}


using MD = MathDefs;
