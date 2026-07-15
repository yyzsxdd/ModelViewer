#pragma once

class  Viewport
{
public:
    explicit Viewport() = default;
    explicit Viewport(int offsetX, int offsetY, int width, int height);
    ~Viewport() = default;
    
    int m_offsetX{};
    int m_offsetY{};
    int m_width{};
    int m_height{};
};

