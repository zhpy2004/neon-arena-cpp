#pragma once

#include <cmath>

struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2& operator+=(const Vec2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vec2& operator-=(const Vec2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vec2& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    Vec2& operator/=(float scalar) noexcept {
        if (scalar == 0.0f) {
            x = 0.0f;
            y = 0.0f;
            return *this;
        }

        x /= scalar;
        y /= scalar;
        return *this;
    }
};

constexpr Vec2 operator+(Vec2 left, const Vec2& right) noexcept {
    return left += right;
}

constexpr Vec2 operator-(Vec2 left, const Vec2& right) noexcept {
    return left -= right;
}

constexpr Vec2 operator*(Vec2 value, float scalar) noexcept {
    return value *= scalar;
}

constexpr Vec2 operator*(float scalar, Vec2 value) noexcept {
    return value *= scalar;
}

inline Vec2 operator/(Vec2 value, float scalar) noexcept {
    return value /= scalar;
}

constexpr float length_squared(const Vec2& value) noexcept {
    return value.x * value.x + value.y * value.y;
}

inline float length(const Vec2& value) noexcept {
    return std::sqrt(length_squared(value));
}

inline Vec2 normalized(const Vec2& value) noexcept {
    const float value_length = length(value);
    return value_length > 0.0f ? value / value_length : Vec2{};
}
