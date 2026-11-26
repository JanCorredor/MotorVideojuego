#include "AABB.h"

bool AABB::Intersects2DXZ(const AABB& other) const
{
	return (min.x <= other.max.x && max.x >= other.min.x) &&
		(min.z <= other.max.z && max.z >= other.min.z);
}

AABB AABB::FromTransformedLocalBox(const glm::vec3& localMin, const glm::vec3& localMax, const glm::mat4& worldMatrix)
{
	AABB result = AABB(localMin, localMax).Transformed(worldMatrix);
	result.min -= glm::vec3(0.001f);
	result.max += glm::vec3(0.001f);
	return result;
}
