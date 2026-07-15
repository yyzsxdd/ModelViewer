#include <layer1/math/Plane.h>
#include <layer1/math/BVHTree.h>
#include <layer1/math/Utils.h>


Plane::Plane(const double normal[3], const double point[3]) noexcept
{
    Define(normal, point);
}

Plane::Plane(const glm::vec3& vec3Normal, float constant) noexcept
    : m_normal(vec3Normal)
    , m_d(constant)
{
    m_absNormal[0] = std::abs(m_normal[0]);
    m_absNormal[1] = std::abs(m_normal[1]);
    m_absNormal[2] = std::abs(m_normal[2]);
}


void Plane::Define(const glm::vec3& normal, const glm::vec3& point)
{
    m_normal = normal;
    m_normal = glm::normalize(m_normal);

    m_absNormal[0] = std::abs(m_normal[0]);
    m_absNormal[1] = std::abs(m_normal[1]);
    m_absNormal[2] = std::abs(m_normal[2]);

    m_d = -glm::dot(m_normal, point);
}

void Plane::Define(const double normal[3], const double point[3])
{
    glm::vec3 glmNormal{ normal[0], normal[1], normal[2] };
    glm::vec3 glmPoint{ point[0], point[1], point[2] };

    Define(glmNormal, glmPoint);
}

void Plane::Define(const double v0[3], const double v1[3], const double v2[3])
{
    glm::vec3 glmV0{ v0[0], v0[1], v0[2] };
    glm::vec3 glmV1{ v1[0], v1[1], v1[2] };
    glm::vec3 glmV2{ v2[0], v2[1], v2[2] };

    Define(glmV0, glmV1, glmV2);
}

void Plane::Define(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2)
{
    glm::vec3 dist1 = v1 - v0;
    glm::vec3 dist2 = v2 - v0;

    Define(glm::cross(dist1, dist2), v0);
}

double Plane::Distance(const glm::vec3& point) const
{
    return glm::dot(m_normal, point) + m_d;
}

Intersection Plane::JudgeLineSegToPlane(const glm::vec3& p1, const glm::vec3& p2) const
{
    bool hasOutside = false;
    bool hasInside = false;

    if (Distance(p1) < 0)
    {
        hasOutside = true;
    }
    else
    {
        hasInside = true;
    }

    if (Distance(p2) < 0)
    {
        hasOutside = true;
    }
    else
    {
        hasInside = true;
    }

    if (hasInside && hasOutside)
    {
        return Intersection::INTERSECTS;
    }

    if (!hasOutside)
    {
        return Intersection::INSIDE;
    }

    return Intersection::OUTSIDE;

}

Intersection Plane::JudgeTriangleToPlane(const std::vector<glm::vec3>& posVec) const
{
    assert(posVec.size() == 3);

    bool hasOuter = false;
    bool hasInner = false;

    auto res = JudgeLineSegToPlane(posVec[0], posVec[1]);
    {
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }
    }

    res = JudgeLineSegToPlane(posVec[1], posVec[2]);
    {
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }
    }

    res = JudgeLineSegToPlane(posVec[0], posVec[2]);
    {
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }
    }


    assert((!hasInner && !hasOuter) == false);
    assert((hasInner && hasOuter) == false);

    if (hasOuter)
    {
        return Intersection::OUTSIDE;
    }

    return Intersection::INSIDE;
}

PlaneD PlaneUtils::Plane2PlaneD(const Plane& plane)
{
    PlaneD ret;
    ret.m_absNormal = plane.m_absNormal;
    ret.m_normal = plane.m_normal;
    ret.m_d = plane.m_d;
    // There is no point defined in Plane, but we can just pick an arbitrary one.
    if (std::abs(plane.m_normal.z) > 1e-6f) {
        ret.m_point = glm::dvec3(.0, .0, -plane.m_d / plane.m_normal.z);
    }
    else if (std::abs(plane.m_normal.y) > 1e-6f) {
        ret.m_point = glm::dvec3(.0, -plane.m_d / plane.m_normal.y, .0);
    }
    else if (std::abs(plane.m_normal.x) > 1e-6f) {
        ret.m_point = glm::dvec3(-plane.m_d / plane.m_normal.x, .0, .0);
    }

    return ret;
}

Intersection PlaneUtils::JudgeTrianglesToPlanes(const std::vector<glm::vec3>& posVec, const std::vector<Plane>& planes)
{
    assert(posVec.size() % 3 == 0);
    bool hasInner = false;
    bool hasOuter = false;

    for (size_t i = 0; i < posVec.size(); i += 3)
    {
        std::vector<glm::vec3> triangle{ posVec[i],posVec[i + 1],posVec[i + 2] };
        Intersection res = JudgeTriangleToPlanes(triangle, planes);
        if (res == Intersection::INTERSECTS)
            return Intersection::INTERSECTS;
        if (res == Intersection::OUTSIDE)
            hasOuter = true;
        else
            hasInner = true;
    }
    assert((hasOuter && hasInner) == false);

    if (hasInner)
        return Intersection::INSIDE;

    return Intersection::OUTSIDE;

}

Intersection PlaneUtils::JudgeTriangleStripToPlanes(const std::vector<glm::vec3>& posVec, const std::vector<PlaneD>& planes)
{
    assert(posVec.size() > 2);

    bool hasInner = false;
    bool hasOuter = false;
    bool isInit = true;
    bool lastTrue = false;
    int lastCount = 0;
    size_t count = posVec.size();
    for (size_t i = 0; i < count - 2; i++)
    {
        int interCount1 = 0;
        int interCount2 = 0;
        Intersection res = JudgeLineSegToPlanes(posVec[i], posVec[i + 1], planes);
        Intersection res2 = JudgeLineSegToPlanes(posVec[i], posVec[i + 2], planes);

        if (!isInit && res == Intersection::OUTSIDE && lastTrue)
        {
            int count1 = GetCountOfLinesegInterPlanes(posVec[i], posVec[i + 1], planes);
            int count2 = GetCountOfLinesegInterPlanes(posVec[i - 1], posVec[i], planes);
            int count3 = GetCountOfLinesegInterPlanes(posVec[i + 1], posVec[i - 1], planes);
            //当三条边都在视椎体外时，若三条边与视椎体构成的四个平面的交点数 相加之和为8，则相交。
            if (count1 + count2 + count3 == 8)
            {
                //增加一次射线判断
                planes[4].m_point;
                double time;
                bool res = rayIntersectsTriangleD(planes[4].m_point, planes[4].m_normal, posVec[i], posVec[i - 1], posVec[i + 1], time);
                if (res)
                {
                    return Intersection::INTERSECTS;
                }
            }
        }

        isInit = false;
        if (res == Intersection::INTERSECTS || res2 == Intersection::INTERSECTS)
            return Intersection::INTERSECTS;
        if (res == Intersection::OUTSIDE || res2 == Intersection::OUTSIDE)
        {
            hasOuter = true;
            if (res == Intersection::OUTSIDE && res2 == Intersection::OUTSIDE)
            {
                //求一个的真实相交点个数，如满足>=2，再求另一个，如>=2则设lastTrue
                int count1 = GetCountOfLinesegInterPlanes(posVec[i], posVec[i + 1], planes);
                if (count1 >= 2)
                {
                    int count2 = GetCountOfLinesegInterPlanes(posVec[i], posVec[i + 2], planes);
                    if (count2 >= 2)
                    {
                        lastTrue = true;
                    }
                    else lastTrue = false;
                }
                else lastTrue = false;
            }
            else  lastTrue = false;
        }
        else
            hasInner = true;
    }
    //余下最后一个线段单独判断
    Intersection res3 = JudgeLineSegToPlanes(posVec[count - 1], posVec[count - 2], planes);
    if (res3 == Intersection::INTERSECTS)
        return Intersection::INTERSECTS;
    if (res3 == Intersection::OUTSIDE)
    {
        hasOuter = true;
        if (lastTrue)
        {
            int count1 = GetCountOfLinesegInterPlanes(posVec[count - 1], posVec[count - 2], planes);
            if (count1 >= 2)
            {
                int count2 = GetCountOfLinesegInterPlanes(posVec[count - 1], posVec[count - 3], planes);
                int count3 = GetCountOfLinesegInterPlanes(posVec[count - 2], posVec[count - 3], planes);
                //当三条边都在视椎体外时，若三条边与视椎体构成的四个平面的交点数 相加之和为8，则相交。
                if (count1 + count2 + count3 == 8)
                {   //增加一次射线判断
                    planes[4].m_point;
                    double time;
                    bool res = rayIntersectsTriangleD(planes[4].m_point, planes[4].m_normal, posVec[count - 1], posVec[count - 2], posVec[count - 3], time);
                    if (res)
                    {
                        return Intersection::INTERSECTS;
                    }
                }
            }
        }
    }
    else
        hasInner = true;

    assert((hasOuter && hasInner) == false);

    if (hasInner)
        return Intersection::INSIDE;

    return Intersection::OUTSIDE;
}

Intersection PlaneUtils::JudgeTriangleToPlanes(const std::vector<glm::vec3>& posVec, const std::vector<Plane>& planes)
{
    assert(posVec.size() == 3);

    bool hasInner = false;
    bool hasInter = false;


    for (auto& plane : planes)
    {
        auto res = plane.JudgeTriangleToPlane(posVec);

        if (res == Intersection::OUTSIDE)
        {
            return Intersection::OUTSIDE;
        }
        else if (res == Intersection::INTERSECTS)
        {
            hasInter = true;
        }
        else
        {
            hasInner = true;
        }
    }

    if (hasInter) return Intersection::INTERSECTS;

    return Intersection::INSIDE;
}

Intersection PlaneUtils::JudgeTriangleToPlanesD(const std::vector<glm::dvec3>& posVec, const std::vector<PlaneD>& planes)
{
    assert(posVec.size() == 3);

    bool hasInner = false;
    bool hasInter = false;


    for (auto& plane : planes)
    {
        auto res = plane.JudgeTriangleToPlane(posVec);

        if (res == Intersection::OUTSIDE)
        {
            return Intersection::OUTSIDE;
        }
        else if (res == Intersection::INTERSECTS)
        {
            hasInter = true;
        }
        else
        {
            hasInner = true;
        }
    }

    if (hasInter) return Intersection::INTERSECTS;

    return Intersection::INSIDE;
}

bool PlaneUtils::JudgePointInCylinder(const glm::vec3& p, const CylinderParams& cyl) {
    glm::vec3 v = p - cyl.start;
    float z = glm::dot(v, cyl.direction);

    // 轴向范围检查 (假设圆柱从 z=0 开始)
    if (z < 0.0f) return false;

    // 径向距离检查
    glm::vec3 vPerp = v - z * cyl.direction;
    float rSq = glm::dot(vPerp, vPerp);
    float radiusSq = cyl.radius * cyl.radius;

    return rSq <= radiusSq;
}

// Möller–Trumbore 射线-三角形求交 (double precision)
bool PlaneUtils::rayIntersectsTriangleD(const glm::dvec3& rayOrigin, const glm::dvec3& rayDir,
                                         const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                         double& t)
{
    const double EPSILON = 1e-12;

    glm::dvec3 E1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
    glm::dvec3 E2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);

    glm::dvec3 P = glm::cross(rayDir, E2);
    double det = glm::dot(E1, P);

    if (std::abs(det) < EPSILON) return false;

    double invDet = 1.0 / det;
    glm::dvec3 T(rayOrigin.x - v0.x, rayOrigin.y - v0.y, rayOrigin.z - v0.z);

    double u = glm::dot(T, P) * invDet;
    if (u < 0.0 || u > 1.0) return false;

    glm::dvec3 Q = glm::cross(T, E1);
    double v = glm::dot(rayDir, Q) * invDet;
    if (v < 0.0 || u + v > 1.0) return false;

    t = glm::dot(E2, Q) * invDet;
    return t > EPSILON;
}

// 辅助函数：线段与圆柱体相交检测
Intersection PlaneUtils::JudgeLineStripToCylinder(const glm::vec3& p0, const glm::vec3& p1, const CylinderParams& cyl) {
    // 1. 快速判断：两个端点都在内部 -> INSIDE
    // 注意：如果线段完全在内部，它不会与侧面相交，但逻辑上属于 INSIDE
    bool p0In = JudgePointInCylinder(p0, cyl);
    bool p1In = JudgePointInCylinder(p1, cyl);

    if (p0In && p1In) {
        return Intersection::INSIDE;
    }

    // 如果两个端点都在外部，需要判断是否相交（穿过）
    if (!p0In && !p1In) {
        // 执行原有的相交检测逻辑
        glm::vec3 d = p1 - p0;
        glm::vec3 v0 = p0 - cyl.start;
        glm::vec3 v1 = p1 - cyl.start;

        float z0 = glm::dot(v0, cyl.direction);
        float z1 = glm::dot(v1, cyl.direction);
        float dz = z1 - z0;

        // 如果线段完全在圆柱起点之前 (z < 0)，则不相交
        if (z1 < 0.0f && z0 < 0.0f) return Intersection::OUTSIDE;

        // 投影到垂直平面
        glm::vec3 v0Perp = v0 - z0 * cyl.direction;
        glm::vec3 v1Perp = v1 - z1 * cyl.direction;

        glm::vec3 dPerp = v1Perp - v0Perp;

        // 二次方程系数
        float a = glm::dot(dPerp, dPerp);
        float b = 2.0f * glm::dot(v0Perp, dPerp);
        float c = glm::dot(v0Perp, v0Perp) - cyl.radius * cyl.radius;

        float discriminant = b * b - 4.0f * a * c;

        if (discriminant < 0.0f) return Intersection::OUTSIDE; // 无实根

        float sqrtDisc = std::sqrt(discriminant);
        float t1 = (-b - sqrtDisc) / (2.0f * a);
        float t2 = (-b + sqrtDisc) / (2.0f * a);

        // 确保 t1 <= t2
        float tMin = std::min(t1, t2);
        float tMax = std::max(t1, t2);

        // 检查交点是否在线段 [0, 1] 范围内
        float tStart = std::max(0.0f, tMin);
        float tEnd = std::min(1.0f, tMax);

        if (tStart > tEnd) return Intersection::OUTSIDE; // 线段与无限圆柱不相交

        // 检查交点处的 Z 坐标是否在圆柱有效范围内 (z >= 0)
        // 线段上的 Z: z(t) = z0 + t * dz
        // 我们需要在 [tStart, tEnd] 区间内存在 t 使得 z(t) >= 0
        // 由于 z(t) 是线性的，只需检查区间端点或区间内是否有 >= 0 的部分

        float zAtStart = z0 + tStart * dz;
        float zAtEnd = z0 + tEnd * dz;

        // 如果区间内任意一点的 z >= 0，则视为相交
        // 情况1: 整个区间 z >= 0
        // 情况2: 区间跨越 z=0 (部分在内部分在外，但这里只关心侧面相交)
        // 只要 zAtEnd >= 0 (假设 tStart <= tEnd)，说明交点区间有有效部分
        if (zAtEnd >= 0.0f) {
            return Intersection::INTERSECTS;
        }

        return Intersection::OUTSIDE;
    }

    // 2. 混合情况：一个在内，一个在外 -> 必然相交
    return Intersection::INTERSECTS;
}

Intersection PlaneUtils::JudgeTriangleStripToCircle(const std::vector<glm::vec3>& posVec, const CylinderParams& cyclinderPar)
{
    //assert(posVec.size() == 3);

    const glm::vec3& v0 = posVec[0];
    const glm::vec3& v1 = posVec[1];
    const glm::vec3& v2 = posVec[2];

    bool in0 = JudgePointInCylinder(v0, cyclinderPar);
    bool in1 = JudgePointInCylinder(v1, cyclinderPar);
    bool in2 = JudgePointInCylinder(v2, cyclinderPar);

    int insideCount = (in0 ? 1 : 0) + (in1 ? 1 : 0) + (in2 ? 1 : 0);

    // 情况 1: 所有顶点都在内部
    if (insideCount == 3) {
        return Intersection::INSIDE;
    }

    // 情况 2: 部分在内部，部分在外部 -> 必然相交
    if (insideCount > 0 && insideCount < 3) {
        return Intersection::INTERSECTS;
    }

    // 情况 3: 所有顶点都在外部
    // 需要检查边是否与圆柱相交
     // 情况 3: 所有顶点都在外部
    if (!in0 && !in1 && !in2) {
        // 检查三条边是否与圆柱体相交
        // 逻辑：只要有一条边返回 INTERSECTS 或 INSIDE，则三角形与圆柱相交
        Intersection res01 = JudgeLineStripToCylinder(v0, v1, cyclinderPar);
        Intersection res12 = JudgeLineStripToCylinder(v1, v2, cyclinderPar);
        Intersection res20 = JudgeLineStripToCylinder(v2, v0, cyclinderPar);

        if (res01 != Intersection::OUTSIDE ||
            res12 != Intersection::OUTSIDE ||
            res20 != Intersection::OUTSIDE) {
            return Intersection::INTERSECTS;
        }


        // 额外检查：三角形是否完全包围了圆柱体的一部分（例如圆柱穿过三角形中心但没碰到边）
        // 这种情况在三角形很小或圆柱很细时较少见，但理论上存在。
        // 更严谨的做法是检查三角形中心到轴线的距离，或者检查圆柱轴线是否与三角形平面相交且在三角形内。
        // 简单优化：如果圆柱半径很大，可能穿过三角形。
        // 这里我们添加一个中心点检查作为兜底（可选，视精度要求而定）
        glm::vec3 center = (v0 + v1 + v2) * 0.333333f;
        if (JudgePointInCylinder(center, cyclinderPar)) {
            // 如果中心在内部，但顶点都在外部，说明圆柱穿过了三角形
            return Intersection::INTERSECTS;
        }

        return Intersection::OUTSIDE;
    }

    // 理论上不会到这里，但为了编译器
    return Intersection::OUTSIDE;
}

Intersection PlaneUtils::JudgeTriangleToCircle(const std::vector<glm::dvec3>& posVec, const CylinderParams& cyclinderPar)
{
    //assert(posVec.size() == 3);

    const glm::vec3& v0 = posVec[0];
    const glm::vec3& v1 = posVec[1];
    const glm::vec3& v2 = posVec[2];

    bool in0 = JudgePointInCylinder(v0, cyclinderPar);
    bool in1 = JudgePointInCylinder(v1, cyclinderPar);
    bool in2 = JudgePointInCylinder(v2, cyclinderPar);

    int insideCount = (in0 ? 1 : 0) + (in1 ? 1 : 0) + (in2 ? 1 : 0);

    // 情况 1: 所有顶点都在内部
    if (insideCount == 3) {
        return Intersection::INSIDE;
    }

    // 情况 2: 部分在内部，部分在外部 -> 必然相交
    if (insideCount > 0 && insideCount < 3) {
        return Intersection::INTERSECTS;
    }

    // 情况 3: 所有顶点都在外部
    // 需要检查边是否与圆柱相交
     // 情况 3: 所有顶点都在外部
    if (!in0 && !in1 && !in2) {
        // 检查三条边是否与圆柱体相交
        // 逻辑：只要有一条边返回 INTERSECTS 或 INSIDE，则三角形与圆柱相交
        Intersection res01 = JudgeLineStripToCylinder(v0, v1, cyclinderPar);
        Intersection res12 = JudgeLineStripToCylinder(v1, v2, cyclinderPar);
        Intersection res20 = JudgeLineStripToCylinder(v2, v0, cyclinderPar);

        if (res01 != Intersection::OUTSIDE ||
            res12 != Intersection::OUTSIDE ||
            res20 != Intersection::OUTSIDE) {
            return Intersection::INTERSECTS;
        }


        // 额外检查：三角形是否完全包围了圆柱体的一部分（例如圆柱穿过三角形中心但没碰到边）
        // 这种情况在三角形很小或圆柱很细时较少见，但理论上存在。
        // 更严谨的做法是检查三角形中心到轴线的距离，或者检查圆柱轴线是否与三角形平面相交且在三角形内。
        // 简单优化：如果圆柱半径很大，可能穿过三角形。
        // 这里我们添加一个中心点检查作为兜底（可选，视精度要求而定）
        glm::vec3 center = (v0 + v1 + v2) * 0.333333f;
        if (JudgePointInCylinder(center, cyclinderPar)) {
            // 如果中心在内部，但顶点都在外部，说明圆柱穿过了三角形
            return Intersection::INTERSECTS;
        }

        return Intersection::OUTSIDE;
    }

    // 理论上不会到这里，但为了编译器
    return Intersection::OUTSIDE;
}





Intersection PlaneUtils::JudgeEdgeToPlanes(const std::vector<glm::vec3>& edgePosVec, const std::vector<Plane>& planes)
{
    assert(edgePosVec.size() % 2 == 0);
    bool hasInner = false;
    bool hasOuter = false;

    for (int i = 0; i < edgePosVec.size(); i += 2)
    {
        for (const auto& plane : planes)
        {
            //判断组成 边 的 线段 与frustum的关系
            auto res = plane.JudgeLineSegToPlane(edgePosVec[i], edgePosVec[i + 1]);
            if (res == Intersection::INTERSECTS)
            {
                return Intersection::INTERSECTS;
            }
            else if (res == Intersection::OUTSIDE)
            {
                hasOuter = true;
            }
            else
            {
                hasInner = true;
            }
        }
    }

    if (hasInner)
        return Intersection::INSIDE;

    return Intersection::OUTSIDE;
}

Intersection PlaneUtils::JudgeTriangleToPlanesD(const glm::dvec3* triangle3Points, const std::vector<PlaneD>& planes) {
    bool hasInner = false;
    bool hasInter = false;

    for (auto& plane : planes)
    {
        auto res = plane.JudgeTriangleToPlane(triangle3Points);

        if (res == Intersection::OUTSIDE)
        {
            return Intersection::OUTSIDE;
        }
        else if (res == Intersection::INTERSECTS)
        {
            hasInter = true;
        }
        else
        {
            hasInner = true;
        }
    }

    if (hasInter) return Intersection::INTERSECTS;

    return Intersection::INSIDE;
}

Intersection PlaneUtils::JudgeLineStripToPlanes(const std::vector<glm::vec3>& edgePosVec, const std::vector<PlaneD>& planes)
{
    assert(edgePosVec.size() > 1);
    bool hasInner = false;
    bool hasOuter = false;

    for (int i = 0; i < edgePosVec.size() - 1; i++)
    {
        auto res = JudgeLineSegToPlanes(edgePosVec[i], edgePosVec[i + 1], planes);
        //判断组成 边 的 线段 与frustum的关系
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }

    }

    if (hasInner)
        return Intersection::INSIDE;

    return Intersection::OUTSIDE;
}

Intersection PlaneUtils::JudgeLineStripToPlanesByCircle(const std::vector<glm::vec3>& edgePosVec, const CylinderParams& cyl)
{
    assert(edgePosVec.size() > 1);
    bool hasInner = false;
    bool hasOuter = false;

    for (int i = 0; i < edgePosVec.size() - 1; i++)
    {
        auto res = JudgeLineStripToCylinder(edgePosVec[i], edgePosVec[i + 1], cyl);
        //判断组成 边 的 线段 与frustum的关系
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }

    }

    if (hasInner)
        return Intersection::INSIDE;

    return Intersection::OUTSIDE;
}

Intersection PlaneUtils::JudgeLineSegToPlanes(const glm::vec3& pos1, const glm::vec3& pos2, const std::vector<PlaneD>& planes)
{
    bool hasIntersect = false;
    bool hasInside = false;

    std::vector<glm::dvec3> inters;
    for (const auto& plane : planes)
    {
        glm::dvec3 intersection(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
        auto res = plane.JudgeLineSegToPlane(pos1, pos2, intersection);
        inters.emplace_back(intersection);
        switch (res)
        {
        case Intersection::OUTSIDE:
            return Intersection::OUTSIDE;
            break;
        case Intersection::INTERSECTS:
            hasIntersect = true;
            break;
        case Intersection::INSIDE:
            hasInside = true;
            break;
        default:
            break;
        }
    }
    //如果与视椎体的四个平面相交，且相交点 在 与这个面相邻的两个面中间，则为相交
    if (hasIntersect)
    {
        bool intersect = false;
        if (JudgePointClipInTwoPlanes(inters[0], planes[1], planes[3]))
        {
            intersect = true;
        }
        else if (JudgePointClipInTwoPlanes(inters[1], planes[0], planes[2]))
        {
            intersect = true;
        }
        else if (JudgePointClipInTwoPlanes(inters[2], planes[1], planes[3]))
        {
            intersect = true;
        }
        else if (JudgePointClipInTwoPlanes(inters[3], planes[0], planes[2]))
        {
            intersect = true;
        }

        if (intersect)
        {
            return Intersection::INTERSECTS;
        }
        else
        {
            return Intersection::OUTSIDE;
        }
    }

    if (hasInside)
    {
        return Intersection::INSIDE;
    }
    return Intersection::OUTSIDE;
}

int PlaneUtils::GetCountOfLinesegInterPlanes(const glm::vec3& pos1, const glm::vec3& pos2, const std::vector<PlaneD>& planes)
{
    int interCount = 0;
    for (int i = 0; i < 4; i++)
    {
        glm::dvec3 intersection(.0, .0, .0);
        auto res = planes[i].JudgeLineSegToPlane(pos1, pos2, intersection);
        if (res == Intersection::INTERSECTS)
        {
            interCount++;
        }
    }
    return interCount;
}


bool PlaneUtils::JudgePointClipInTwoPlanes(const glm::vec3& pos, const PlaneD& plane1, const PlaneD& plane2)
{
    if (pos.x == std::numeric_limits<double>::max() && pos.y == std::numeric_limits<double>::max() && pos.z == std::numeric_limits<double>::max())
    {
        return false;
    }
    else
    {
        return (plane1.Distance(pos) > 0) && (plane2.Distance(pos) > 0);
    }

}



PlaneD::PlaneD(const double normal[3], const double point[3]) noexcept
{
    Define(normal, point);
}

void PlaneD::Define(const glm::dvec3& normal, const glm::dvec3& point)
{
    m_normal = normal;
    m_normal = glm::normalize(m_normal);

    m_absNormal[0] = std::abs(m_normal[0]);
    m_absNormal[1] = std::abs(m_normal[1]);
    m_absNormal[2] = std::abs(m_normal[2]);

    m_point = point;

    m_d = -glm::dot(m_normal, point);
}

void PlaneD::Define(const double normal[3], const double point[3])
{
    glm::dvec3 glmNormal{ normal[0], normal[1], normal[2] };
    glm::dvec3 glmPoint{ point[0], point[1], point[2] };

    Define(glmNormal, glmPoint);
}

void PlaneD::Define(const double v0[3], const double v1[3], const double v2[3])
{
    glm::vec3 glmV0{ v0[0], v0[1], v0[2] };
    glm::vec3 glmV1{ v1[0], v1[1], v1[2] };
    glm::vec3 glmV2{ v2[0], v2[1], v2[2] };

    Define(glmV0, glmV1, glmV2);
}

void PlaneD::Define(const glm::dvec3& v0, const glm::dvec3& v1, const glm::dvec3& v2)
{
    glm::vec3 dist1 = v1 - v0;
    glm::vec3 dist2 = v2 - v0;

    Define(glm::cross(dist1, dist2), v0);
}

double PlaneD::Distance(const glm::dvec3& point) const
{
    return glm::dot(m_normal, point) + m_d;
}

Intersection PlaneD::JudgeLineSegToPlane(const glm::dvec3& p1, const glm::dvec3& p2, glm::dvec3& intersection) const
{

    bool hasOutside = false;
    bool hasInside = false;

    if (Distance(p1) < 0)
    {
        hasOutside = true;
    }
    else
    {
        hasInside = true;
    }

    if (Distance(p2) < 0)
    {
        hasOutside = true;
    }
    else
    {
        hasInside = true;
    }

    if (hasInside && hasOutside)
    {
        // 计算线段的方向向量  
        double dirX = p2.x - p1.x;
        double dirY = p2.y - p1.y;
        double dirZ = p2.z - p1.z;

        // 计算平面方程中线段的t参数  
        double denominator = (m_normal.x * dirX + m_normal.y * dirY + m_normal.z * dirZ);


        // 计算与平面的交点参数t  
        double t = -(m_normal.x * p1.x + m_normal.y * p1.y + m_normal.z * p1.z + m_d) / denominator;
        assert(t >= .0 && t <= 1.0);
        // 计算交点  
        intersection.x = p1.x + t * dirX;
        intersection.y = p1.y + t * dirY;
        intersection.z = p1.z + t * dirZ;

        return Intersection::INTERSECTS;
    }

    if (!hasOutside)
    {
        return Intersection::INSIDE;
    }

    return Intersection::OUTSIDE;


}

Intersection PlaneD::JudgeTriangleToPlane(const glm::dvec3* triangle3Point) const {

    bool hasOuter = false;
    bool hasInner = false;
    glm::dvec3 intersection1, intersection2, intersection3;
    auto res = JudgeLineSegToPlane(triangle3Point[0], triangle3Point[1], intersection1);
    {
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }
    }

    res = JudgeLineSegToPlane(triangle3Point[1], triangle3Point[2], intersection2);
    {
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }
    }

    res = JudgeLineSegToPlane(triangle3Point[0], triangle3Point[2], intersection3);
    {
        if (res == Intersection::INTERSECTS)
        {
            return Intersection::INTERSECTS;
        }
        else if (res == Intersection::OUTSIDE)
        {
            hasOuter = true;
        }
        else
        {
            hasInner = true;
        }
    }


    assert((!hasInner && !hasOuter) == false);
    assert((hasInner && hasOuter) == false);

    if (hasOuter)
    {
        return Intersection::OUTSIDE;
    }

    return Intersection::INSIDE;
}

Intersection PlaneD::JudgeTriangleToPlane(const std::vector<glm::dvec3>& posVec) const
{
    assert(posVec.size() == 3);
    return JudgeTriangleToPlane(posVec.data());
}