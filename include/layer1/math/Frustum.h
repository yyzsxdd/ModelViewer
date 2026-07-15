#pragma once

#include "BoundingBox.hpp"
#include "MathDefs.h"
#include "Plane.h"

/// Frustum planes.
enum FrustumPlane
{
    PLANE_NEAR = 0,
    PLANE_LEFT,
    PLANE_RIGHT,
    PLANE_UP,
    PLANE_DOWN,
    PLANE_FAR,
};

#define NUM_FRUSTUM_PLANES 6
#define NUM_FRUSTUM_VERTICES 8

class Frustum
{
public:
    /// Construct a degenerate frustum with all points at origin.
    explicit Frustum() noexcept = default;

    /// Define frustum by vertices, be caution with the sequence: vertices define start from right top, clockwise on near plane, then clockwise on far plane.
    bool Define(const std::array<glm::vec3, NUM_FRUSTUM_VERTICES>& vertices);
    /// Define frustum by the right-top corner points on both the near clip and far clip planes, and with a camera transform.
    bool Define(const glm::vec3& near, const glm::vec3& far, const glm::mat4& transform);
    /// Define with projection parameters and a transform matrix, only for perspective projection mode.
    bool Define(float fov, float aspectRatio, float nearZ, float farZ, const glm::mat4& transform);
    /// Define with projection parameters and a transform matrix, only for orthographic projection mode.
    bool DefineOrtho(float orthoSize, float aspectRatio, float nearZ, float farZ, const glm::mat4& transform);

    /// Test if a bounding box is inside, outside or intersects.
    Intersection IsInside(const BoundingBox<double>& box) const;

    /// Update the planes. Called internally.
    bool UpdatePlanes();

    /// Frustum planes. Plane normal oriented towards inside.
    Plane m_planes[NUM_FRUSTUM_PLANES];

    glm::vec3 m_vertices[NUM_FRUSTUM_VERTICES];
};

