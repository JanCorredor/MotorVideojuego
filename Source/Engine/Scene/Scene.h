#pragma once

#include "../Module.h"
#include "GameObjects/GameObject.h"
#include "../Render/Primitives/GeometryGenerator.h"
#include "../Geometry/AABB.h"
#include "../Geometry/Frustum.h"
#include "../ResourceManagement/Resource.h"
#include <vector>
#include <memory>
#include <string>
#include <array>
#include <unordered_map>
#include <unordered_set>

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
	// Crea un GameObject por nodo de un modelo importado (ResourceModel)
	GameObject* InstantiateModel(UID modelUID, GameObject* parent = nullptr);
	// El ResourceManager avisa de que estos recursos se han borrado: los componentes los sueltan
	void OnResourcesRemoved(const std::vector<UID>& uids);
	GameObject* DuplicateGameObject(GameObject* target, GameObject* parent = nullptr);

	// Destrucción recursiva en cascada y segura de GameObjects
	void DestroyGameObject(GameObject* target);

	// Archivo soltado en la ventana: modelo => importa, textura => se asigna al seleccionado
	void HandleDroppedFile(const std::string& filePath);

	GameObject* GetRoot() { return &sceneRoot; }
	const GameObject* GetRoot() const { return &sceneRoot; }
	GameObject* GetMainCameraObject() const { return mainCameraPtr; }
	GameObject* GetGameCameraObject() const { return gameCameraPtr; }

	// Selección múltiple. El activo (el último seleccionado) es el que usan Inspector, cámara y drag & drop.
	// El root y objetos que no pertenezcan a la escena se rechazan.
	void SetSelected(GameObject* object) { Select(object); }
	GameObject* GetSelected() const { return selected; }
	const std::vector<GameObject*>& GetSelection() const { return selection; }
	bool HasSelection() const { return !selection.empty(); }
	bool IsSelected(const GameObject* object) const { return selectionSet.count(object) > 0; }
	// Seleccionado o descendiente de un seleccionado (contorno del render)
	bool IsInSelectedHierarchy(const GameObject* object) const;
	void Select(GameObject* object);               // sustituye la selección (nullptr = ninguna)
	void AddToSelection(GameObject* object);       // la amplía y lo hace activo
	void RemoveFromSelection(GameObject* object);
	void ToggleSelection(GameObject* object);      // Ctrl + clic
	void SetSelection(const std::vector<GameObject*>& objects, GameObject* active = nullptr);
	void ClearSelection() { Select(nullptr); }
	void SelectAll();
	// Sin los que ya cuelgan de otro seleccionado: es lo que se duplica, borra o mueve
	std::vector<GameObject*> GetTopLevelSelection() const;
	std::vector<uint64_t> GetSelectionUIDs() const;
	void SelectByUIDs(const std::vector<uint64_t>& uids, uint64_t activeUID = 0);
	// Duplica o borra la selección de nivel superior; tras duplicar quedan seleccionadas las copias
	std::vector<GameObject*> DuplicateSelection();
	void DestroySelection();
	uint64_t GetSelectionRevision() const { return selectionRevision; }
	void NotifySelectionHierarchyChanged() { selectionRevision++; }

	// Comprueba si un puntero a GameObject es propiedad viva de esta escena (O(1) por el mapa de UID)
	bool OwnsObject(const GameObject* object) const;
	GameObject* FindByUID(uint64_t uid) const;

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

	// Cambios de escena pedidos durante el frame: se hacen al principio del siguiente (PreUpdate), cuando
	// nadie tiene punteros a GameObjects a medio usar. El editor usa estos y no Load / NewScene directamente.
	// Si se pide otra escena, la restauración pendiente de Stop se descarta (salvo que la carga falle).
	void RequestLoadScene(const std::string& filePath);
	void RequestNewScene();
	void RequestRestoreTemporaryScene(const std::string& filePath);
	bool HasPendingSceneChange() const { return pendingSceneChange != PendingSceneChange::None || !pendingRestorePath.empty(); }

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
	void ProcessPendingSceneChange();
	// Vacía la escena (sin cámara del editor ni árboles); la usan cargar, nueva escena y CleanUp
	void ClearScene();

	enum class PendingSceneChange { None, Load, New };
	PendingSceneChange pendingSceneChange = PendingSceneChange::None;
	std::string pendingScenePath;
	std::string pendingRestorePath; // escena temporal de Stop por restaurar

	GameObject sceneRoot;
	std::vector<std::unique_ptr<GameObject>> sceneEntities;
	std::unordered_map<uint64_t, GameObject*> objectsByUID; // solo objetos vivos (no encolados para destruir)
	std::vector<GameObject*> pendingDestruction;

	Quadtree quadtree;
	Octree octree;
	bool spacePartitioningDirty = true;
	int treeMaxObjectsPerNode = 1;
	int treeMaxDepth = 5;

	std::unique_ptr<GameObject> editorCameraObj;
	GameObject* mainCameraPtr = nullptr;
	GameObject* gameCameraPtr = nullptr;
	GameObject* selected = nullptr;                          // objeto activo de la selección
	std::vector<GameObject*> selection;                      // en el orden en que se seleccionaron
	std::unordered_set<const GameObject*> selectionSet;      // IsSelected en O(1): el render lo consulta por objeto
	uint64_t selectionRevision = 0;
	mutable std::string currentScenePath;
	mutable bool sceneDirty = false;
	mutable bool isLoadingOrInitializing = false;
	mutable std::string prePlayScenePath;
	mutable bool prePlaySceneDirty = false;
};
