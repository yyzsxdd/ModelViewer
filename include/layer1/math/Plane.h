#pragma once

#include "MathDefs.h"
#include <glm/glm.hpp>
#include <vector>

struct CylinderParams;


class  Plane
{
public:
    explicit Plane() noexcept = default;

    /// Construct from a normal vector and a point on the plane.
    Plane(const double normal[3], const double point[3]) noexcept;

    
    /// Construct from a normal vector and distance to the origin.
    Plane(const glm::vec3& vec3Normal, float constant) noexcept;
       

    /// Define with a normal vector and a point on the plane.
    void Define(const double normal[3], const double point[3]);
    void Define(const glm::vec3& normal, const glm::vec3& point);

    /// Define with three points.
    void Define(const double v0[3], const double v1[3], const double v2[3]);
    void Define(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

    double Distance(const glm::vec3& point) const;
    //jueg relation between line segment and plane
    Intersection JudgeLineSegToPlane(const glm::vec3& p1, const glm::vec3& p2) const ;

    Intersection JudgeTriangleToPlane(const std::vector<glm::vec3>& posVec) const ;



    /// Plane normal.
    glm::vec3 m_normal;
    /// Plane absolute normal.
    glm::vec3 m_absNormal;
    /// Plane constant.
    double m_d{};
};

class  PlaneD
{
public:
    explicit PlaneD() noexcept = default;

    /// Construct from a normal vector and a point on the plane.
    PlaneD(const double normal[3], const double point[3]) noexcept;

    /// Define with a normal vector and a point on the plane.
    void Define(const double normal[3], const double point[3]);
    void Define(const glm::dvec3& normal, const glm::dvec3& point);

    /// Define with three points.
    void Define(const double v0[3], const double v1[3], const double v2[3]);
    void Define(const glm::dvec3& v0, const glm::dvec3& v1, const glm::dvec3& v2);

    double Distance(const glm::dvec3& point) const;
    //jueg relation between line segment and plane
    Intersection JudgeLineSegToPlane(const glm::dvec3& p1, const glm::dvec3& p2, glm::dvec3& intersect) const;

    Intersection JudgeTriangleToPlane(const std::vector<glm::dvec3>& posVec) const;
    Intersection JudgeTriangleToPlane(const glm::dvec3* triangle3Point) const;

    /// Plane normal.
    glm::dvec3 m_normal;
    /// Plane absolute normal.
    glm::dvec3 m_absNormal;
    /// Plane constant.
    double m_d{};
    glm::dvec3 m_point;
};

struct  PlaneUtils {

    static PlaneD Plane2PlaneD(const Plane& plane);

    //判断 面 与frustum的关系
    static Intersection JudgeTrianglesToPlanes(const std::vector<glm::vec3>& posVec, const std::vector<Plane>& planes);
    //判断triangleStrip与frustum的关系
    static Intersection JudgeTriangleStripToPlanes(const std::vector<glm::vec3>& posVec, const std::vector<PlaneD>& planes);
    static Intersection JudgeTriangleStripToCircle(const std::vector<glm::vec3>& posVec, const CylinderParams& cyclinderPar);
    //判断一个 三角形 与frustum的关系
    static Intersection JudgeTriangleToPlanes(const std::vector<glm::vec3>& posVec, const std::vector<Plane>& planes);
    
    static Intersection JudgeTriangleToPlanesD(const std::vector<glm::dvec3>& posVec, const std::vector<PlaneD>& planes);

    static Intersection JudgeTriangleToCircle(const std::vector<glm::dvec3>& posVec, const CylinderParams& cyclinderPar);

    static Intersection JudgeTriangleToPlanesD(const glm::dvec3* triangle3Points, const std::vector<PlaneD>& planes);

    //判断一整条 边 与frustum的关系,传入数据为点对
    static Intersection JudgeEdgeToPlanes(const std::vector<glm::vec3>& edgePosVec, const std::vector<Plane>& planes);
    //判断一整条 边 与frustum的关系，传入数据为点strip
    static Intersection JudgeLineStripToPlanes(const std::vector<glm::vec3>& edgePosVec, const std::vector<PlaneD>& planes);

    static Intersection JudgeLineStripToPlanesByCircle(const std::vector<glm::vec3>& edgePosVec, const CylinderParams& cyl);

    static Intersection JudgeLineSegToPlanes(const glm::vec3& pos1, const glm::vec3& pos2, const std::vector<PlaneD>& planes);

    static int GetCountOfLinesegInterPlanes(const glm::vec3& pos1, const glm::vec3& pos2, const std::vector<PlaneD>& planes);

    static bool JudgePointClipInTwoPlanes(const glm::vec3& pos, const PlaneD& plane1, const PlaneD& plane2);

    static Intersection JudgeLineStripToCylinder(const glm::vec3& p0, const glm::vec3& p1, const CylinderParams& cyl);

    static bool JudgePointInCylinder(const glm::vec3& p, const CylinderParams& cyl);

    /// 射线与三角形求交 (double precision)
    static bool rayIntersectsTriangleD(const glm::dvec3& rayOrigin, const glm::dvec3& rayDir,
                                       const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                       double& t);

};
