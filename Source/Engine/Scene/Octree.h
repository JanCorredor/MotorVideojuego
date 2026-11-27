#pragma once

#include "../Geometry/AABB.h"
#include "../Geometry/Frustum.h"
#include "../Geometry/Ray.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

class GameObject;

// Coste de una consulta: se muestra en el editor para ver el ahorro del árbol
struct OctreeQueryStats
{
	int nodesVisited = 0;  // nodos comprobados
	int objectsTested = 0; // objetos cuya AABB se ha comprobado una a una
};

// ============================================================================
// OCTREE (partición espacial 3D de los objetos Static)
// Cada objeto se guarda una sola vez, en el nodo más pequeño que contiene su AABB entera.
// Recorridos iterativos con pila.
// ============================================================================
class Octree
{
public:
	struct Entry
	{
		GameObject* object = nullptr;
		AABB box; // AABB en espacio de mundo
	};

	void Build(const std::vector<Entry>& entries, int maxObjectsPerNode, int maxDepth);
	void Clear();

	// Objetos cuya AABB no queda fuera del frustum (un nodo entero dentro no prueba sus objetos)
	void CollectVisible(const Frustum& frustum, std::vector<GameObject*>& outObjects, OctreeQueryStats* stats = nullptr) const;
	// (distancia, objeto) de las AABB que atraviesa el rayo, sin ordenar
	void CollectRayHits(const Ray& ray, std::vector<std::pair<float, GameObject*>>& outHits, OctreeQueryStats* stats = nullptr) const;
	void CollectNodeBoxes(std::vector<AABB>& outBoxes) const;

	bool IsEmpty() const { return root == nullptr; }
	AABB GetBounds() const { return root ? root->box : AABB(); }
	int GetMaxObjectsPerNode() const { return maxObjects; }
	int GetMaxDepth() const { return maxTreeDepth; }
	size_t GetNodeCount() const { return nodeCount; }
	int GetDepthReached() const { return depthReached; }
	size_t GetObjectCount() const { return objectCount; }

private:
	struct Node
	{
		AABB box;
		int depth = 0;
		std::vector<Entry> entries;                    // objetos que no caben enteros en ningún hijo
		std::array<std::unique_ptr<Node>, 8> children; // vacíos mientras es hoja

		bool IsLeaf() const { return children[0] == nullptr; }
	};

	void Insert(const Entry& entry);
	void Subdivide(Node& node);
	static int FindContainingChild(const Node& node, const AABB& box);

	std::unique_ptr<Node> root;
	int maxObjects = 1;
	int maxTreeDepth = 5;
	size_t nodeCount = 0;
	int depthReached = 0;
	size_t objectCount = 0;
};
