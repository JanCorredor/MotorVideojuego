#pragma once

#include "AABB.h"
#include <glm/glm.hpp>
#include <array>

// dot(normal, p) + distance = 0. La normal apunta hacia dentro del frustum
struct Plane
{
	glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
	float distance = 0.0f;

	// > 0 dentro, < 0 fuera
	float SignedDistance(const glm::vec3& point) const { return glm::dot(normal, point) + distance; }
};

enum class FrustumTestResult
{
	Outside,
	Intersect,
	Inside
};

// Lo que ve una cámara: 6 planos y 8 esquinas sacados de Proyección * Vista
class Frustum
{
public:
	Frustum() = default;
	explicit Frustum(const glm::mat4& viewProjection) { Set(viewProjection); }

	void Set(const glm::mat4& viewProjection);

	FrustumTestResult Test(const AABB& box) const;
	bool IsVisible(const AABB& box) const { return Test(box) != FrustumTestResult::Outside; }

	// Mismo orden de bits que AABB::GetCorners(); bit 2 = 0 plano cercano, 1 lejano
	const std::array<glm::vec3, 8>& GetCorners() const { return corners; }

	bool IsValid() const { return valid; }

private:
	std::array<Plane, 6> planes{}; // izquierdo, derecho, inferior, superior, cercano, lejano
	std::array<glm::vec3, 8> corners{};
	bool valid = false;
};
