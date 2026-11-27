#include "Octree.h"

#include <algorithm>
#include <stack>

void Octree::Clear()
{
	root.reset(); // la profundidad está acotada por maxTreeDepth: destruir en cascada es seguro
	nodeCount = 0;
	depthReached = 0;
	objectCount = 0;
}

void Octree::Build(const std::vector<Entry>& entries, int maxObjectsPerNode, int maxDepth)
{
	Clear();
	maxObjects = std::max(1, maxObjectsPerNode);
	maxTreeDepth = std::max(1, maxDepth);

	AABB bounds;
	for (const Entry& entry : entries)
	{
		if (entry.object) bounds.Enclose(entry.box);
	}
	if (!bounds.IsValid()) return;

	// Raíz cúbica (hijos regulares) y un 1% más grande para que nada quede en el borde por redondeo
	const glm::vec3 size = bounds.GetSize();
	const glm::vec3 half(std::max({ size.x, size.y, size.z, 0.01f }) * 0.5f * 1.01f);
	const glm::vec3 center = bounds.GetCenter();

	root = std::make_unique<Node>();
	root->box = AABB(center - half, center + half);
	nodeCount = 1;

	for (const Entry& entry : entries)
	{
		if (!entry.object || !entry.box.IsValid()) continue;
		Insert(entry);
		++objectCount;
	}
}

void Octree::Insert(const Entry& entry)
{
	Node* node = root.get();
	while (true)
	{
		if (node->IsLeaf())
		{
			if ((int)node->entries.size() < maxObjects || node->depth >= maxTreeDepth)
			{
				node->entries.push_back(entry);
				return;
			}

			// Hoja llena: cada hijo recibe como mucho maxObjects, así que no hace falta volver a dividir
			Subdivide(*node);
			std::vector<Entry> previous;
			previous.swap(node->entries);
			for (const Entry& old : previous)
			{
				const int c = FindContainingChild(*node, old.box);
				if (c >= 0) node->children[c]->entries.push_back(old);
				else node->entries.push_back(old); // ocupa varios hijos: se queda en el padre
			}
		}

		const int c = FindContainingChild(*node, entry.box);
		if (c < 0)
		{
			node->entries.push_back(entry);
			return;
		}
		node = node->children[c].get();
	}
}

void Octree::Subdivide(Node& node)
{
	const glm::vec3 minP = node.box.min;
	const glm::vec3 maxP = node.box.max;
	const glm::vec3 center = node.box.GetCenter();

	// Cada bit del índice elige la mitad del eje: bit0 = X, bit1 = Y, bit2 = Z
	for (int i = 0; i < 8; ++i)
	{
		auto child = std::make_unique<Node>();
		child->box.min = glm::vec3((i & 1) ? center.x : minP.x, (i & 2) ? center.y : minP.y, (i & 4) ? center.z : minP.z);
		child->box.max = glm::vec3((i & 1) ? maxP.x : center.x, (i & 2) ? maxP.y : center.y, (i & 4) ? maxP.z : center.z);
		child->depth = node.depth + 1;
		node.children[i] = std::move(child);
	}
	nodeCount += 8;
	depthReached = std::max(depthReached, node.depth + 1);
}

int Octree::FindContainingChild(const Node& node, const AABB& box)
{
	if (node.IsLeaf()) return -1;

	// Candidato: el hijo que contiene el centro de la caja; luego se comprueba que la contenga entera
	const glm::vec3 nodeCenter = node.box.GetCenter();
	const glm::vec3 boxCenter = box.GetCenter();
	const int index = (boxCenter.x >= nodeCenter.x ? 1 : 0) |
		(boxCenter.y >= nodeCenter.y ? 2 : 0) |
		(boxCenter.z >= nodeCenter.z ? 4 : 0);

	return node.children[index]->box.Contains(box) ? index : -1;
}

void Octree::CollectVisible(const Frustum& frustum, std::vector<GameObject*>& outObjects, OctreeQueryStats* stats) const
{
	if (!root) return;

	// Par (nodo, ya está entero dentro del frustum)
	std::stack<std::pair<const Node*, bool>> stack;
	stack.push({ root.get(), false });

	while (!stack.empty())
	{
		const auto [node, parentInside] = stack.top();
		stack.pop();
		if (stats) stats->nodesVisited++;

		bool inside = parentInside;
		if (!inside)
		{
			const FrustumTestResult result = frustum.Test(node->box);
			if (result == FrustumTestResult::Outside) continue; // se descarta toda la rama
			inside = (result == FrustumTestResult::Inside);
		}

		for (const Entry& entry : node->entries)
		{
			if (inside)
			{
				outObjects.push_back(entry.object);
				continue;
			}
			if (stats) stats->objectsTested++;
			if (frustum.IsVisible(entry.box)) outObjects.push_back(entry.object);
		}

		if (!node->IsLeaf())
		{
			for (const std::unique_ptr<Node>& child : node->children)
			{
				stack.push({ child.get(), inside });
			}
		}
	}
}

void Octree::CollectRayHits(const Ray& ray, std::vector<std::pair<float, GameObject*>>& outHits, OctreeQueryStats* stats) const
{
	if (!root) return;

	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();

		float distance = 0.0f;
		if (!Intersection::RayAABB(ray, node->box, distance)) continue; // el rayo no pasa por este nodo
		if (stats) stats->nodesVisited++;

		for (const Entry& entry : node->entries)
		{
			if (stats) stats->objectsTested++;
			if (Intersection::RayAABB(ray, entry.box, distance)) outHits.emplace_back(distance, entry.object);
		}

		if (!node->IsLeaf())
		{
			for (const std::unique_ptr<Node>& child : node->children)
			{
				stack.push(child.get());
			}
		}
	}
}

void Octree::CollectNodeBoxes(std::vector<AABB>& outBoxes) const
{
	outBoxes.clear();
	if (!root) return;

	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		outBoxes.push_back(node->box);

		if (!node->IsLeaf())
		{
			for (const std::unique_ptr<Node>& child : node->children)
			{
				stack.push(child.get());
			}
		}
	}
}
