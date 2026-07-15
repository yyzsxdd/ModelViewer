#pragma once

#include "MathDefs.h"
#include <glm/glm.hpp>

template <typename T>
class BoundingBox
{
public:
    BoundingBox();
    BoundingBox(const T min[3], const T max[3]);
    BoundingBox(const glm::vec<3, T, glm::defaultp>& min, const glm::vec<3, T, glm::defaultp>& max);

    bool IsValid() const
    {
        return (!(m_min[0] > m_max[0] || m_min[1] > m_max[1] || m_min[2] > m_max[2])) && MD::CheckValidNumber(m_min.x, m_min.y, m_min.z, m_max.x, m_max.y, m_max.z);
    }

    bool SetMinMax(const T min[3], const T max[3]);
    bool SetMinMax(const glm::vec<3, T, glm::defaultp>& min, const glm::vec<3, T, glm::defaultp>& max);

    bool Merge(const T point[3]);
    bool Merge(T x, T y, T z);
    bool Merge(const BoundingBox<T>& box);
    bool Merge(const glm::vec<3, T, glm::defaultp>& pos);
    bool GetCenter(T out[3]) const;

    bool Clear();

    Intersection IsInside(const BoundingBox<T>& box) const;

    bool operator==(const BoundingBox<T>& box) const;

    union {
        glm::vec<3, T, glm::defaultp> m_min;
        glm::vec<3, T, glm::defaultp> min;
    };
    union {
        glm::vec<3, T, glm::defaultp> m_max;
        glm::vec<3, T, glm::defaultp> max;
    };
};

template <typename T>
BoundingBox<T>::BoundingBox()
{
    if (std::numeric_limits<T>::has_infinity)
    {
        const T& c_infinity = std::numeric_limits<T>::infinity();
        m_min[0] = m_min[1] = m_min[2] = c_infinity;
        m_max[0] = m_max[1] = m_max[2] = -c_infinity;
    }
    else
    {
#       undef min
#       undef max
        m_min[0] = m_min[1] = m_min[2] = std::numeric_limits<T>::min();
        m_max[0] = m_max[1] = m_max[2] = std::numeric_limits<T>::max();
    }
}

template <typename T>
BoundingBox<T>::BoundingBox(const T min[3], const T max[3])
{
    SetMinMax(min, max);
}

template <typename T>
BoundingBox<T>::BoundingBox(const glm::vec<3, T, glm::defaultp>& min, const glm::vec<3, T, glm::defaultp>& max)
{
    SetMinMax(min, max);
}

template <typename T>
bool BoundingBox<T>::SetMinMax(const T min[3], const T max[3])
{
    m_min[0] =  min[0];
    m_min[1] =  min[1];
    m_min[2] =  min[2];

    m_max[0] = max[0];
    m_max[1] = max[1];
    m_max[2] = max[2];

    return true;
}

template <typename T>
bool BoundingBox<T>::SetMinMax(const glm::vec<3, T, glm::defaultp>& min, const glm::vec<3, T, glm::defaultp>& max) {
    m_min = min;
    m_max = max;
    return true;
}

template <typename T>
bool BoundingBox<T>::Merge(const T point[3])
{
    return Merge(point[0], point[1], point[2]);
}

template <typename T>
bool BoundingBox<T>::Merge(T x, T y, T z)
{
    if (x < m_min[0])
        m_min[0] = x;
    if (y < m_min[1])
        m_min[1] = y;
    if (z < m_min[2])
        m_min[2] = z;
    if (x > m_max[0])
        m_max[0] = x;
    if (y > m_max[1])
        m_max[1] = y;
    if (z > m_max[2])
        m_max[2] = z;

    return true;
}

template <typename T>
bool BoundingBox<T>::Merge(const BoundingBox<T>& box)
{
    if (box.IsValid()) {
        if (box.m_min[0] < m_min[0])
            m_min[0] = box.m_min[0];
        if (box.m_min[1] < m_min[1])
            m_min[1] = box.m_min[1];
        if (box.m_min[2] < m_min[2])
            m_min[2] = box.m_min[2];
        if (box.m_max[0] > m_max[0])
            m_max[0] = box.m_max[0];
        if (box.m_max[1] > m_max[1])
            m_max[1] = box.m_max[1];
        if (box.m_max[2] > m_max[2])
            m_max[2] = box.m_max[2];

        return true;
    }

    return false;
}

template <typename T>
bool BoundingBox<T>::Merge(const glm::vec<3, T, glm::defaultp>& pos)
{
    if (pos.x < m_min[0]) m_min[0] = pos.x;
    if (pos.y < m_min[1]) m_min[1] = pos.y;
    if (pos.z < m_min[2]) m_min[2] = pos.z;
    if (pos.x > m_max[0]) m_max[0] = pos.x;
    if (pos.y > m_max[1]) m_max[1] = pos.y;
    if (pos.z > m_max[2]) m_max[2] = pos.z;
    return true;
}

template <typename T>
bool BoundingBox<T>::Clear()
{
    m_min[0] = m_min[1] = m_min[2] = MathDefs::FloatInfinity();
    m_max[0] = m_max[1] = m_max[2] = -MathDefs::FloatInfinity();

    return true;
}

template <typename T>
bool BoundingBox<T>::GetCenter(T out[3]) const
{
    out[0] = (m_min[0] + m_max[0]) * 0.5f;
    out[1] = (m_min[1] + m_max[1]) * 0.5f;
    out[2] = (m_min[2] + m_max[2]) * 0.5f;

    return true;
}

template <typename T>
Intersection BoundingBox<T>::IsInside(const BoundingBox<T>& box) const
{
    if (box.m_max[0] < m_min[0] || box.m_min[0] > m_max[0] || box.m_max[1] < m_min[1] || box.m_min[1] > m_max[1] ||
        box.m_max[2] < m_min[2] || box.m_min[2] > m_max[2]) {
        
        return Intersection::OUTSIDE;
    } else if (box.m_min[0] < m_min[0] || box.m_max[0] > m_max[0] || box.m_min[1] < m_min[1] || box.m_max[1] > m_max[1] ||
        box.m_min[2] < m_min[2] || box.m_max[2] > m_max[2]) {
        
        return Intersection::INTERSECTS;
    } else {
        return Intersection::INSIDE;
    }
}

template <typename T>
bool BoundingBox<T>::operator==(const BoundingBox<T>& box) const {
    return this->m_min == box.min && this->m_max == box.max;
}

using BoundingBoxF = BoundingBox<float>;
using BoundingBoxD = BoundingBox<double>;

