#pragma once

#include <glm/glm.hpp>

class Color
{
public:
    Color() = default;
    Color(float r, float g, float b, float a = 1.f);
    Color(double r, double g, double b, double a = 1.f);
    Color(const glm::vec4& color);
    Color(const Color& color);

    Color& operator =(const Color& color);
    Color& operator =(Color&& color);

    bool operator ==(const Color& color) const;
    bool operator !=(const Color& color) const;

    /// Multiply with a scalar.
    Color operator *(float rhs) const { return Color(m_r * rhs, m_g * rhs, m_b * rhs, m_a * rhs); }

    /// Convert to glm::vec3, the alpha chanel is discarded.
    glm::vec3 ToVec3() const;
    /// Convert to glm::vec4.
    glm::vec4 ToVec4() const;
    /// Convert to uint32_t. 8 bit for each channel, and is mapped to 0~255 range.
    uint32_t ToUInt32() const;

    // rgb: le: 0x00BBGGRR, 字节序: RR GG BB 00
    static glm::vec3 ColorRGBConvertToVec3f(uint32_t rgb);

    // 
    static uint32_t ColorRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return (((uint32_t)a) << 24) | (((uint32_t)b) << 16) | (((uint32_t)g) << 8) | (((uint32_t)r) << 0);
    }

    //颜色计算（不同亮度）
    static glm::vec3 CalculateColor(glm::vec3 color, float brightnessFactor);

    static glm::vec3 RgbToHsv(glm::vec3 rgb);

    static glm::vec3 HsvToRgb(glm::vec3 hsv);

    static glm::vec4 ConvertColorRGBA(uint32_t rgba);

    union {
        float m_r;
        float r;
    };

    union {
        float m_g;
        float g;
    };

    union {
        float m_b;
        float b;
    };

    union {
        float m_a;
        float a;
    };

    static const Color WHITE;
    static const Color BLACK;
    static const Color GRAY;
    static const Color RED;
    static const Color DRAK_RED;
    static const Color GREEN;
    static const Color BLUE;
};

