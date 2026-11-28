#pragma once

#include "../Module.h"
#include "GameObjects/GameObject.h"
#include "../Render/Primitives/GeometryGenerator.h"
#include "../Geometry/AABB.h"
#include "../Geometry/Frustum.h"
#include "Octree.h"
#include "../ResourceManagement/Resource.h"
#include <vector>
#include <memory>
#include <string>
#include <array>
#include <unordered_map>
#include <unordered_set>

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

	// Space Partitioning (Octree para objetos Static)
	void MarkSpacePartitioningDirty() { spacePartitioningDirty = true; }
	void RebuildSpacePartitioning();
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
	// Pedir otra escena saca del Play: la restauración de Stop se descarta, salvo que la carga falle.
	void RequestLoadScene(const std::string& filePath);
	void RequestNewScene();
	bool HasPendingSceneChange() const { return pendingSceneChange != PendingSceneChange::None || pendingPlayRestore; }

	const std::string& GetCurrentScenePath() const { return currentScenePath; }
	void SetCurrentScenePath(const std::string& path);
	bool HasSavedScenePath() const { return !currentScenePath.empty(); }
	std::string GetSceneDisplayName() const;

	bool IsSceneDirty() const { return sceneDirty; }
	void MarkSceneDirty();
	void ClearSceneDirty();
	void UpdateWindowTitle() const;

	// Copia en memoria de la escena al pulsar Play; Stop la restaura al principio del siguiente frame
	bool SavePlaySnapshot();
	void RequestRestorePlaySnapshot();
	size_t GetPlaySnapshotSize() const { return playSnapshot.size(); }

private:

	void ProcessPendingDestructions();
	void ProcessPendingSceneChange();
	void RestorePlaySnapshot();
	// Escena <-> JSON sin tocar ficheros. editorState: cámara del editor y selección guardadas (vacío = la cámara no cambia)
	nlohmann::json SerializeToJson(const std::string& sceneName) const;
	bool LoadFromJson(const nlohmann::json& root, const std::string& sourceName, const nlohmann::json& editorState);
	// Vacía la escena (sin cámara del editor ni árboles); la usan cargar, nueva escena y CleanUp
	void ClearScene();

	enum class PendingSceneChange { None, Load, New };
	PendingSceneChange pendingSceneChange = PendingSceneChange::None;
	std::string pendingScenePath;
	std::string playSnapshot;        // escena en JSON al pulsar Play (vacía fuera del Play)
	bool pendingPlayRestore = false; // Stop pendiente de restaurarla
	bool prePlaySceneDirty = false;

	GameObject sceneRoot;
	std::vector<std::unique_ptr<GameObject>> sceneEntities;
	std::unordered_map<uint64_t, GameObject*> objectsByUID; // solo objetos vivos (no encolados para destruir)
	std::vector<GameObject*> pendingDestruction;

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
};
