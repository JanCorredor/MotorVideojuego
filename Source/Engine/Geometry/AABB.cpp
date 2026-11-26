#include "AABB.h"

void AABB::Encapsulate(const glm::vec3& point)
{
	minPoint = glm::min(minPoint, point);
	maxPoint = glm::max(maxPoint, point);
}

void AABB::Encapsulate(const AABB& other)
{
	minPoint = glm::min(minPoint, other.minPoint);
	maxPoint = glm::max(maxPoint, other.maxPoint);
}

bool AABB::Intersects(const AABB& other) const
{
	return (minPoint.x <= other.maxPoint.x && maxPoint.x >= other.minPoint.x) &&
		(minPoint.y <= other.maxPoint.y && maxPoint.y >= other.minPoint.y) &&
		(minPoint.z <= other.maxPoint.z && maxPoint.z >= other.minPoint.z);
}

bool AABB::Intersects2DXZ(const AABB& other) const
{
	return (minPoint.x <= other.maxPoint.x && maxPoint.x >= other.minPoint.x) &&
		(minPoint.z <= other.maxPoint.z && maxPoint.z >= other.minPoint.z);
}

bool AABB::IntersectsRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float& tMinOut) const
{
	float tmin = -FLT_MAX;
	float tmax = FLT_MAX;

	const float eps = 1e-7f;
	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::fabs(rayDir[axis]) < eps)
		{
			if (rayOrigin[axis] < minPoint[axis] || rayOrigin[axis] > maxPoint[axis])
			{
				return false;
			}
		}
		else
		{
			float invD = 1.0f / rayDir[axis];
			float t0 = (minPoint[axis] - rayOrigin[axis]) * invD;
			float t1 = (maxPoint[axis] - rayOrigin[axis]) * invD;
			if (t0 > t1) std::swap(t0, t1);

			tmin = std::max(tmin, t0);
			tmax = std::min(tmax, t1);
			if (tmax < tmin) return false;
		}
	}

	if (tmax >= 0.0f)
	{
		tMinOut = (tmin >= 0.0f) ? tmin : tmax;
		return true;
	}
	return false;
}

AABB AABB::FromTransformedLocalBox(const glm::vec3& localMin, const glm::vec3& localMax, const glm::mat4& worldMatrix)
{
	const glm::vec3 corners[8] = {
		glm::vec3(localMin.x, localMin.y, localMin.z),
		glm::vec3(localMax.x, localMin.y, localMin.z),
		glm::vec3(localMin.x, localMax.y, localMin.z),
		glm::vec3(localMax.x, localMax.y, localMin.z),
		glm::vec3(localMin.x, localMin.y, localMax.z),
		glm::vec3(localMax.x, localMin.y, localMax.z),
		glm::vec3(localMin.x, localMax.y, localMax.z),
		glm::vec3(localMax.x, localMax.y, localMax.z)
	};

	glm::vec3 firstWorld = glm::vec3(worldMatrix * glm::vec4(corners[0], 1.0f));
	AABB result(firstWorld, firstWorld);

	for (int i = 1; i < 8; ++i)
	{
		glm::vec3 worldCorner = glm::vec3(worldMatrix * glm::vec4(corners[i], 1.0f));
		result.Encapsulate(worldCorner);
	}

	result.minPoint -= glm::vec3(0.001f);
	result.maxPoint += glm::vec3(0.001f);
	return result;
}