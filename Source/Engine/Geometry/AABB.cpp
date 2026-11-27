#include "AABB.h"

AABB AABB::FromTransformedLocalBox(const glm::vec3& localMin, const glm::vec3& localMax, const glm::mat4& worldMatrix)
{
	AABB result = AABB(localMin, localMax).Transformed(worldMatrix);
	result.min -= glm::vec3(0.001f);
	result.max += glm::vec3(0.001f);
	return result;
}
