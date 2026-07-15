#pragma once

#include <glm/glm.hpp>

class Plane;

struct RayTestResult {
    bool intersected;
    float distance;
    RayTestResult(bool i, float d) : intersected(i), distance(d) {}
};

struct RayD {
    glm::dvec3 origin;
    glm::dvec3 direction;
};

class Ray
{
public:
    explicit Ray() = default;
    ~Ray() = default;

    Ray(const glm::vec3& ori, const glm::vec3& dir);
   
    
    glm::vec3 getPoint(float t) const;

    RayTestResult intersects(const Plane& plane) const;

    glm::vec3 getProjectionPoint(const glm::vec3& point) const;

    glm::vec3 m_origin;
    glm::vec3 m_direction;

};

