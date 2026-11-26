#include "Frustum.h"

#include <cmath>

void FrustumPlane::Normalize()
{
	float len = glm::length(normal);
	if (len > 1e-6f)
	{
		normal /= len;
		d /= len;
	}
}

Frustum Frustum::FromViewProjection(const glm::mat4& m)
{
	Frustum frustum;

	// Left
	frustum.planes[0].normal.x = m[0][3] + m[0][0];
	frustum.planes[0].normal.y = m[1][3] + m[1][0];
	frustum.planes[0].normal.z = m[2][3] + m[2][0];
	frustum.planes[0].d        = m[3][3] + m[3][0];

	// Right
	frustum.planes[1].normal.x = m[0][3] - m[0][0];
	frustum.planes[1].normal.y = m[1][3] - m[1][0];
	frustum.planes[1].normal.z = m[2][3] - m[2][0];
	frustum.planes[1].d        = m[3][3] - m[3][0];

	// Bottom
	frustum.planes[2].normal.x = m[0][3] + m[0][1];
	frustum.planes[2].normal.y = m[1][3] + m[1][1];
	frustum.planes[2].normal.z = m[2][3] + m[2][1];
	frustum.planes[2].d        = m[3][3] + m[3][1];

	// Top
	frustum.planes[3].normal.x = m[0][3] - m[0][1];
	frustum.planes[3].normal.y = m[1][3] - m[1][1];
	frustum.planes[3].normal.z = m[2][3] - m[2][1];
	frustum.planes[3].d        = m[3][3] - m[3][1];

	// Near
	frustum.planes[4].normal.x = m[0][3] + m[0][2];
	frustum.planes[4].normal.y = m[1][3] + m[1][2];
	frustum.planes[4].normal.z = m[2][3] + m[2][2];
	frustum.planes[4].d        = m[3][3] + m[3][2];

	// Far
	frustum.planes[5].normal.x = m[0][3] - m[0][2];
	frustum.planes[5].normal.y = m[1][3] - m[1][2];
	frustum.planes[5].normal.z = m[2][3] - m[2][2];
	frustum.planes[5].d        = m[3][3] - m[3][2];

	for (size_t i = 0; i < frustum.planes.size(); ++i)
	{
		frustum.planes[i].Normalize();
	}

	return frustum;
}

bool Frustum::IntersectsAABB(const AABB& box) const
{
	for (size_t i = 0; i < planes.size(); ++i)
	{
		const FrustumPlane& plane = planes[i];
		glm::vec3 positiveVertex(
			(plane.normal.x >= 0.0f) ? box.maxPoint.x : box.minPoint.x,
			(plane.normal.y >= 0.0f) ? box.maxPoint.y : box.minPoint.y,
			(plane.normal.z >= 0.0f) ? box.maxPoint.z : box.minPoint.z
		);

		if (plane.DistanceToPoint(positiveVertex) < 0.0f)
		{
			return false;
		}
	}
	return true;
}

std::array<glm::vec3, 8> Frustum::ExtractCorners(const glm::mat4& viewProj)
{
	glm::mat4 invVP = glm::inverse(viewProj);
	const glm::vec4 ndcCorners[8] = {
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f),
		glm::vec4( 1.0f, -1.0f, -1.0f, 1.0f),
		glm::vec4( 1.0f,  1.0f, -1.0f, 1.0f),
		glm::vec4(-1.0f,  1.0f, -1.0f, 1.0f),
		glm::vec4(-1.0f, -1.0f,  1.0f, 1.0f),
		glm::vec4( 1.0f, -1.0f,  1.0f, 1.0f),
		glm::vec4( 1.0f,  1.0f,  1.0f, 1.0f),
		glm::vec4(-1.0f,  1.0f,  1.0f, 1.0f)
	};

	std::array<glm::vec3, 8> worldCorners;
	for (size_t i = 0; i < 8; ++i)
	{
		glm::vec4 worldPt = invVP * ndcCorners[i];
		if (std::fabs(worldPt.w) > 1e-6f)
		{
			worldCorners[i] = glm::vec3(worldPt) / worldPt.w;
		}
		else
		{
			worldCorners[i] = glm::vec3(worldPt);
		}
	}
	return worldCorners;
}
