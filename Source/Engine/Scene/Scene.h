#pragma once

#include "../Module.h"
#include "GameObjects/GameObject.h"
#include "../Render/Primitives/GeometryGenerator.h"
#include <vector>
#include <memory>
#include <string>
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

// ============================================================================
// QUADTREE (Partición espacial 2D en el plano XZ, conservando altura Y)
// Implementado íntegramente de forma iterativa mediante std::stack
// ============================================================================
class Quadtree
{
public:
	struct Entry
	{
		GameObject* object = nullptr;
		AABB box;
	};

	struct Node
	{
		AABB bounds;
		int depth = 0;
		std::vector<Entry> objects;
		std::array<std::unique_ptr<Node>, 4> children;

		bool IsLeaf() const { return children[0] == nullptr; }
	};

	Quadtree() = default;
	~Quadtree() = default;

	void Build(const AABB& worldBounds, const std::vector<Entry>& staticEntries, int maxObjectsPerNode = 2, int maxDepth = 5);
	void Clear();

	int QueryFrustum(const Frustum& frustum, std::vector<GameObject*>& outObjects) const;
	int QueryRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, std::vector<GameObject*>& outObjects) const;

	void CollectNodeBounds(std::vector<AABB>& outBoxes) const;

	int GetMaxObjectsPerNode() const { return maxObjects; }
	int GetMaxDepth() const { return maxTreeDepth; }
	size_t GetNodeCount() const { return totalNodes; }
	size_t GetObjectCount() const { return totalObjects; }

private:
	void SubdivideNode(Node* node);

	std::unique_ptr<Node> root;
	int maxObjects = 2;
	int maxTreeDepth = 5;
	size_t totalNodes = 0;
	size_t totalObjects = 0;
};

// ============================================================================
// OCTREE (Partición espacial 3D en los ejes X, Y, Z)
// Implementado íntegramente de forma iterativa mediante std::stack
// ============================================================================
class Octree
{
public:
	using Entry = Quadtree::Entry;

	struct Node
	{
		AABB bounds;
		int depth = 0;
		std::vector<Entry> objects;
		std::array<std::unique_ptr<Node>, 8> children;

		bool IsLeaf() const { return children[0] == nullptr; }
	};

	Octree() = default;
	~Octree() = default;

	void Build(const AABB& worldBounds, const std::vector<Entry>& staticEntries, int maxObjectsPerNode = 2, int maxDepth = 5);
	void Clear();

	int QueryFrustum(const Frustum& frustum, std::vector<GameObject*>& outObjects) const;
	int QueryRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, std::vector<GameObject*>& outObjects) const;

	void CollectNodeBounds(std::vector<AABB>& outBoxes) const;

	int GetMaxObjectsPerNode() const { return maxObjects; }
	int GetMaxDepth() const { return maxTreeDepth; }
	size_t GetNodeCount() const { return totalNodes; }
	size_t GetObjectCount() const { return totalObjects; }

private:
	void SubdivideNode(Node* node);

	std::unique_ptr<Node> root;
	int maxObjects = 2;
	int maxTreeDepth = 5;
	size_t totalNodes = 0;
	size_t totalObjects = 0;
};

class Scene : public Module
{
public:
	Scene();
	virtual ~Scene();

	bool Awake() override;
	bool Start() override;
	bool PreUpdate() override;
	bool Update(float dt) override;
	bool PostUpdate() override;
	bool CleanUp() override;

	// Creación de GameObjects con soporte para padre opcional (nullptr = cuelga de sceneRoot)
	GameObject* CreateNewGameObject(std::vector<std::unique_ptr<Component>> objComponents, const std::string& name = "GameObject", GameObject* parent = nullptr);
	GameObject* CreateEmptyGameObject(GameObject* parent = nullptr);
	GameObject* CreatePrimitive(PrimitiveType type, GameObject* parent = nullptr);
	GameObject* ImportModel(const std::string& filePath, GameObject* parent = nullptr);
	GameObject* DuplicateGameObject(GameObject* target, GameObject* parent = nullptr);

	// Destrucción recursiva en cascada y segura de GameObjects
	void DestroyGameObject(GameObject* target);

	// Archivo soltado en la ventana: modelo => importa, textura => se asigna al seleccionado
	void HandleDroppedFile(const std::string& filePath);

	GameObject* GetRoot() { return &sceneRoot; }
	const GameObject* GetRoot() const { return &sceneRoot; }
	GameObject* GetMainCameraObject() const { return mainCameraPtr; }
	GameObject* GetGameCameraObject() const { return gameCameraPtr; }

	// Selección única de la escena (la consumen Editor y cámara). nullptr = sin selección.
	// El root y objetos que no pertenezcan a la escena se rechazan.
	void SetSelected(GameObject* object);
	GameObject* GetSelected() const { return selected; }
	uint64_t GetSelectionRevision() const { return selectionRevision; }
	void NotifySelectionHierarchyChanged() { selectionRevision++; }

	// Comprueba si un puntero a GameObject es propiedad viva de esta escena
	bool OwnsObject(const GameObject* object) const;

	std::string MakeUniqueName(const std::string& baseName) const;
	GameObject* Register(std::unique_ptr<GameObject> newObj, GameObject* parent = nullptr);

	// Space Partitioning (Quadtree & Octree para objetos Static)
	void MarkSpacePartitioningDirty() { spacePartitioningDirty = true; }
	void RebuildSpacePartitioning();
	const Quadtree& GetQuadtree() const { return quadtree; }
	const Octree& GetOctree() const { return octree; }

	int GetTreeMaxObjects() const { return treeMaxObjectsPerNode; }
	int GetTreeMaxDepth() const { return treeMaxDepth; }
	void SetTreeParameters(int maxObjectsPerNode, int maxDepth);

	// Serialización de Escena: Save(file) y Load(file) en formato JSON + Custom File Format (.mesh)
	bool Save(const std::string& filePath) const;
	bool Load(const std::string& filePath);
	void NewScene();

	const std::string& GetCurrentScenePath() const { return currentScenePath; }
	void SetCurrentScenePath(const std::string& path);
	bool HasSavedScenePath() const { return !currentScenePath.empty(); }
	std::string GetSceneDisplayName() const;

	bool IsSceneDirty() const { return sceneDirty; }
	void MarkSceneDirty();
	void ClearSceneDirty();
	void UpdateWindowTitle() const;

	// Guardado y restauración de escena temporal (Play / Stop)
	bool SaveTemporaryScene(const std::string& filePath) const;
	bool LoadTemporaryScene(const std::string& filePath, bool deleteFileAfterLoad = true);

private:
	
	void ProcessPendingDestructions();

	GameObject sceneRoot;
	std::vector<std::unique_ptr<GameObject>> sceneEntities;
	std::vector<GameObject*> pendingDestruction;

	Quadtree quadtree;
	Octree octree;
	bool spacePartitioningDirty = true;
	int treeMaxObjectsPerNode = 1;
	int treeMaxDepth = 5;

	std::unique_ptr<GameObject> editorCameraObj;
	GameObject* mainCameraPtr = nullptr;
	GameObject* gameCameraPtr = nullptr;
	GameObject* selected = nullptr;
	uint64_t selectionRevision = 0;
	mutable std::string currentScenePath;
	mutable bool sceneDirty = false;
	mutable bool isLoadingOrInitializing = false;
	mutable std::string prePlayScenePath;
	mutable bool prePlaySceneDirty = false;
};
