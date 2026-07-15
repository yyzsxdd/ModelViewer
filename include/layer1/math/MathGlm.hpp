#pragma once

#include "MathDefs.h"
#include <glm/glm.hpp>


typedef glm::ivec2 Point;
typedef glm::ivec2 Size;

struct Math {

    static inline bool Contains(const Point& rectPt, const Size& rectSize, const Point& pt) {
        return (
            pt.x >= rectPt.x &&
            pt.x <= rectPt.x + rectSize.x &&
            pt.y >= rectPt.y &&
            pt.y <= rectPt.y + rectSize.y
            );
    }

    static inline glm::vec3 CalculateBasicFaceNormal(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3) {
        auto normal = glm::cross((v2 - v1), (v3 - v1));
        return glm::normalize(normal);

    }

    static inline float AbsDot(const glm::vec3& v1, const glm::vec3& v2) {
        return fabs(v1.x * v2.x) + fabs(v1.y * v2.y) + fabs(v1.z * v2.z);
    }

  
    template<typename T>
    static T* Max(const T* begin, const T* end) {
        auto result = begin;
        for (auto itor = begin + 1; itor != end; ++itor) {
            if (*result < *itor) {
                result = itor;
            }
        }
        return result;
    }

    // rect: x, y, width, height; return value: 1: inside, -1: outside, 0: onside
    static int RectContains(const glm::ivec4& rect, const glm::ivec2& pt) {

        const auto l = pt.x - rect.x;
        const auto r = pt.x - (rect.x + rect.z);
        auto resultX = l * r;       // >0: outside, <0: inside, ==0: onside;

        const auto t = pt.y - rect.y;
        const auto b = pt.y - (rect.y + rect.w);
        auto resultY = t * b;       // >0: outside, <0: inside, ==0: onside;

        if (resultX < 0 && resultY < 0) { return 1; }
        if ((resultX == 0 && resultY < 0) || (resultY == 0 && resultX < 0)) { return 0; }
        return -1;
    }

    static glm::ivec4 RectMerge(const glm::ivec4& rect0, const glm::ivec4& rect1) {

        const auto x00 = MathDefs::Min(rect0.x, rect0.x + rect0.z);
        const auto y00 = MathDefs::Min(rect0.y, rect0.y + rect0.w);
        const auto x01 = MathDefs::Max(rect0.x, rect0.x + rect0.z);
        const auto y01 = MathDefs::Max(rect0.y, rect0.y + rect0.w);

        const auto x10 = MathDefs::Min(rect1.x, rect1.x + rect1.z);
        const auto y10 = MathDefs::Min(rect1.y, rect1.y + rect1.w);
        const auto x11 = MathDefs::Max(rect1.x, rect1.x + rect1.z);
        const auto y11 = MathDefs::Max(rect1.y, rect1.y + rect1.w);

        const auto x0 = MathDefs::Min(x00, x10);
        const auto y0 = MathDefs::Min(y00, y10);

        const auto x1 = MathDefs::Max(x01, x11);
        const auto y1 = MathDefs::Max(y01, y11);

        return glm::ivec4(x0, y0, x1 - x0, y1 - y0);

    }

};
