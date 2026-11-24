#pragma once

#include <glm/glm.hpp>

// Caja envolvente alineada con los ejes (Axis-Aligned Bounding Box) en espacio de mundo
struct AABB
{
	glm::vec3 minPoint = glm::vec3(0.0f);
	glm::vec3 maxPoint = glm::vec3(0.0f);

	AABB() = default;
	AABB(const glm::vec3& minP, const glm::vec3& maxP) : minPoint(minP), maxPoint(maxP) {}

	bool IsValid() const
	{
		return minPoint.x <= maxPoint.x && minPoint.y <= maxPoint.y && minPoint.z <= maxPoint.z;
	}

	glm::vec3 GetCenter() const { return (minPoint + maxPoint) * 0.5f; }
	glm::vec3 GetExtents() const { return (maxPoint - minPoint) * 0.5f; }
	glm::vec3 GetSize() const { return maxPoint - minPoint; }

	void Encapsulate(const glm::vec3& point);
	void Encapsulate(const AABB& other);

	bool Intersects(const AABB& other) const;
	bool Intersects2DXZ(const AABB& other) const;
	bool IntersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float& tMinOut) const;

	static AABB FromTransformedLocalBox(const glm::vec3& localMin, const glm::vec3& localMax, const glm::mat4& worldMatrix);
};