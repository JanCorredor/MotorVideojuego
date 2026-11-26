#pragma once

#include "AABB.h"
#include <glm/glm.hpp>
#include <array>

// Plano geométrico (normal * X + d = 0) para el Frustum de cámara
struct FrustumPlane
{
	glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
	float d = 0.0f;

	void Normalize();
	float DistanceToPoint(const glm::vec3& point) const
	{
		return glm::dot(normal, point) + d;
	}
};

// Frustum de visualización compuesto por 6 planos extraídos de la matriz View-Projection
struct Frustum
{
	std::array<FrustumPlane, 6> planes;

	static Frustum FromViewProjection(const glm::mat4& viewProj);
	bool IntersectsAABB(const AABB& box) const;
	static std::array<glm::vec3, 8> ExtractCorners(const glm::mat4& viewProj);
};
