#include "Frustum.h"

void Frustum::Set(const glm::mat4& viewProjection)
{
	valid = true;

	// GLM guarda por columnas: la fila i es (m[0][i], m[1][i], m[2][i], m[3][i])
	const auto row = [&viewProjection](int i)
	{
		return glm::vec4(viewProjection[0][i], viewProjection[1][i], viewProjection[2][i], viewProjection[3][i]);
	};
	const glm::vec4 r0 = row(0);
	const glm::vec4 r1 = row(1);
	const glm::vec4 r2 = row(2);
	const glm::vec4 r3 = row(3);

	// Gribb-Hartmann: cada desigualdad -w <= x,y,z <= w del espacio clip es un plano
	const glm::vec4 equations[6] = {
		r3 + r0, // izquierdo
		r3 - r0, // derecho
		r3 + r1, // inferior
		r3 - r1, // superior
		r3 + r2, // cercano
		r3 - r2  // lejano
	};

	for (int i = 0; i < 6; ++i)
	{
		const glm::vec3 normal(equations[i]);
		const float length = glm::length(normal);
		if (length < 1e-8f)
		{
			valid = false; // Matriz degenerada (p. ej. aspect ratio 0)
			planes[i] = Plane();
			continue;
		}
		planes[i].normal = normal / length;
		planes[i].distance = equations[i].w / length;
	}

	// Esquinas: se deshace la proyección del cubo NDC [-1, 1]^3
	const glm::mat4 inverse = glm::inverse(viewProjection);
	for (int i = 0; i < 8; ++i)
	{
		const glm::vec4 ndc((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f, 1.0f);
		const glm::vec4 world = inverse * ndc;
		corners[i] = (world.w != 0.0f) ? glm::vec3(world) / world.w : glm::vec3(world);
	}
}

FrustumTestResult Frustum::Test(const AABB& box) const
{
	if (!valid || !box.IsValid()) return FrustumTestResult::Outside;

	bool intersecting = false;
	for (const Plane& plane : planes)
	{
		// Esquina más avanzada según la normal: si queda fuera, toda la caja está fuera
		const glm::vec3 positive(plane.normal.x >= 0.0f ? box.max.x : box.min.x,
			plane.normal.y >= 0.0f ? box.max.y : box.min.y,
			plane.normal.z >= 0.0f ? box.max.z : box.min.z);
		if (plane.SignedDistance(positive) < 0.0f) return FrustumTestResult::Outside;

		// Esquina opuesta: si queda fuera, la caja cruza el plano
		const glm::vec3 negative(plane.normal.x >= 0.0f ? box.min.x : box.max.x,
			plane.normal.y >= 0.0f ? box.min.y : box.max.y,
			plane.normal.z >= 0.0f ? box.min.z : box.max.z);
		if (plane.SignedDistance(negative) < 0.0f) intersecting = true;
	}
	return intersecting ? FrustumTestResult::Intersect : FrustumTestResult::Inside;
}
