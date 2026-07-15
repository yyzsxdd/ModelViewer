#pragma once

#include <array>

class IntRect
{
public:
    IntRect() = default;

    /// Create by start point and endpoint, left, top, right and bottom are automatically set.
    IntRect(const std::array<int, 2>& start, const std::array<int, 2>& end);
    
    /// Create by left, top, right and bottom, the start point is set to (left, top), the end point is set to (right, bottom)
    IntRect(int left, int top, int right, int bottom);

    bool Define(const std::array<int, 2>& start, const std::array<int, 2>& end);

    bool Define(int left, int top, int right, int bottom);
    
    ~IntRect() = default;

    std::array<int, 2> m_start;
    std::array<int, 2> m_end;
    
    int m_left{};
    int m_top{};
    int m_right{};
    int m_bottom{};
};

