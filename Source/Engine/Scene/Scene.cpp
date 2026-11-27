#include "Scene.h"

#include "../Engine.h"
#include "../Window/Window.h"
#include "../Render/Render.h"
#include "../Input/Input.h"
#include "../../Utils/Log.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/MeshComponent.h"
#include "GameObjects/Components/MaterialComponent.h"
#include "GameObjects/Components/CameraComponent.h"
#include "../Render/Primitives/GeometryGenerator.h"

#include "../ResourceManagement/Importers/ImporterManager.h"
#include "../ResourceManagement/ResourceManager.h"
#include "../ResourceManagement/ResourceModel.h"

#include "../Geometry/AABB.h"
#include "../Geometry/Ray.h"
#include "../FileSystem/FileSystem.h"

#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <stack>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cctype>

#ifdef _DEBUG
#include <tracy/Tracy.hpp>
#endif


Scene::Scene() : Module()
{
	name = "scene";
}

Scene::~Scene()
{
}

bool Scene::Awake()
{
	LOG("[Scene] Inicializando módulo Scene...");
	return true;
}

bool Scene::Start()
{
	LOG("[Scene] Configurando escena por defecto...");
	isLoadingOrInitializing = true;

	// Instance Editor Camera
	editorCameraObj = std::make_unique<GameObject>("EditorCamera");
	auto camTrans = std::make_unique<TransformComponent>(editorCameraObj.get());
	camTrans->SetPosition(glm::vec4(0.0f, 1.0f, 5.0f, 1.0f));
	editorCameraObj->AddComponent(std::move(camTrans));

	auto camComp = std::make_unique<CameraComponent>(editorCameraObj.get());
	editorCameraObj->AddComponent(std::move(camComp));

	mainCameraPtr = editorCameraObj.get();
	LOG("[Scene] Instanciada 'EditorCamera' interna en posición (0.0, 1.0, 5.0)");

	// Instance Game Camera (Main Camera para la ventana Game)
	auto gameCamObj = std::make_unique<GameObject>("Main Camera");
	auto gameCamTrans = std::make_unique<TransformComponent>(gameCamObj.get());
	gameCamTrans->SetPosition(glm::vec3(0.0f, 2.0f, 7.0f));
	gameCamTrans->SetRotation(glm::vec3(-10.0f, 0.0f, 0.0f));
	gameCamObj->AddComponent(std::move(gameCamTrans));

	auto gameCamComp = std::make_unique<CameraComponent>(gameCamObj.get());
	gameCamObj->AddComponent(std::move(gameCamComp));

	gameCameraPtr = gameCamObj.get();
	sceneRoot.AddChild(gameCamObj.get());
	sceneEntities.push_back(std::move(gameCamObj));
	LOG("[Scene] Instanciada 'Main Camera' para la vista Game");



	RebuildSpacePartitioning();
	SetSelected(nullptr);

	currentScenePath.clear();
	sceneDirty = false;
	isLoadingOrInitializing = false;
	UpdateWindowTitle();

	LOG("[Scene] Escena inicial cargada con éxito (%zu entidades registradas)", sceneEntities.size());

	return true;
}

bool Scene::PreUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	ProcessPendingDestructions();
	return true;
}

bool Scene::Update(float dt)
{
#ifdef _DEBUG
	ZoneScoped;
#endif

	// Centralización del drag & drop de archivos
	if (Engine::GetInstance().input && Engine::GetInstance().input->HasDroppedFile())
	{
		std::string droppedPath = Engine::GetInstance().input->GetDroppedFilePath();
		HandleDroppedFile(droppedPath);
		Engine::GetInstance().input->ClearDroppedFile();
	}
	if (editorCameraObj)
	{
		editorCameraObj->Update(dt);
	}
	sceneRoot.Update(dt);
	ProcessPendingDestructions();

	if (spacePartitioningDirty)
	{
		RebuildSpacePartitioning();
	}
	return true;
}

bool Scene::PostUpdate()
{
#ifdef _DEBUG
	ZoneScoped;
#endif
	return true;
}

bool Scene::CleanUp()
{
	LOG("[Scene] Limpiando entidades de la escena (%zu entidades)...", sceneEntities.size());
	const std::string& tempFile = Engine::GetInstance().GetTempSceneFilePath();
	std::error_code ec;
	if (!tempFile.empty() && std::filesystem::exists(tempFile, ec))
	{
		std::filesystem::remove(tempFile, ec);
	}
	quadtree.Clear();
	octree.Clear();
	pendingDestruction.clear();
	selected = nullptr;
	mainCameraPtr = nullptr;
	gameCameraPtr = nullptr;
	editorCameraObj.reset();
	sceneEntities.clear();
	return true;
}

std::string Scene::MakeUniqueName(const std::string& baseName) const
{
	bool exists = false;
	for (const std::unique_ptr<GameObject>& entity : sceneEntities)
	{
		if (entity->GetName() == baseName)
		{
			exists = true;
			break;
		}
	}
	if (!exists) return baseName;

	int index = 1;
	while (true)
	{
		std::string candidate = baseName + " (" + std::to_string(index) + ")";
		bool found = false;
		for (const std::unique_ptr<GameObject>& entity : sceneEntities)
		{
			if (entity->GetName() == candidate)
			{
				found = true;
				break;
			}
		}
		if (!found) return candidate;
		index++;
	}
}

bool Scene::OwnsObject(const GameObject* object) const
{
	if (!object || object == &sceneRoot) return false;

	// Si está encolado para destrucción, ya no se considera disponible
	for (const GameObject* pending : pendingDestruction)
	{
		if (pending == object) return false;
	}

	for (const std::unique_ptr<GameObject>& entity : sceneEntities)
	{
		if (entity.get() == object) return true;
	}
	return false;
}

void Scene::SetSelected(GameObject* object)
{
	if (object == nullptr)
	{
		if (selected != nullptr)
		{
			selected = nullptr;
			selectionRevision++;
		}
		return;
	}
	if (OwnsObject(object))
	{
		selected = object;
		selectionRevision++;
	}
	else
	{
		selected = nullptr;
		selectionRevision++;
	}
}

GameObject* Scene::Register(std::unique_ptr<GameObject> newObj, GameObject* parent)
{
	GameObject* ptr = newObj.get();
	if (parent != nullptr && (parent == &sceneRoot || OwnsObject(parent)))
	{
		parent->AddChild(ptr);
	}
	else
	{
		sceneRoot.AddChild(ptr);
	}
	sceneEntities.push_back(std::move(newObj));
	spacePartitioningDirty = true;
	if (!isLoadingOrInitializing)
	{
		MarkSceneDirty();
	}
	return ptr;
}

GameObject* Scene::CreateNewGameObject(std::vector<std::unique_ptr<Component>> objComponents, const std::string& name, GameObject* parent)
{
	std::string uniqueName = MakeUniqueName(name);
	auto newObj = std::make_unique<GameObject>(uniqueName);

	std::unordered_set<ComponentType> presentTypes;
	for (std::unique_ptr<Component>& comp : objComponents)
	{
		if (comp)
		{
			presentTypes.insert(comp->GetType());
			newObj->AddComponent(std::move(comp));
		}
	}

	if (presentTypes.find(COMPONENT_TRANSFORM) == presentTypes.end())
	{
		auto newTrans = std::make_unique<TransformComponent>(newObj.get());
		newTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
		newObj->AddComponent(std::move(newTrans));
	}

	if (presentTypes.find(COMPONENT_MESH) == presentTypes.end())
	{
		auto newMesh = std::make_unique<MeshComponent>(newObj.get(), ResourceManager::GetBuiltInMeshUID(PrimitiveType::Cube));
		newObj->AddComponent(std::move(newMesh));
	}

	if (presentTypes.find(COMPONENT_MATERIAL) == presentTypes.end())
	{
		auto cubeMat = std::make_unique<MaterialComponent>(newObj.get());
		cubeMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		cubeMat->SetUseTexture(false);
		newObj->AddComponent(std::move(cubeMat));
	}

	GameObject* registered = Register(std::move(newObj), parent);
	LOG("[Scene] GameObject '%s' creado (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::CreateEmptyGameObject(GameObject* parent)
{
	std::string uniqueName = MakeUniqueName("GameObject");
	auto newObj = std::make_unique<GameObject>(uniqueName);

	auto newTrans = std::make_unique<TransformComponent>(newObj.get());
	newTrans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	newObj->AddComponent(std::move(newTrans));

	GameObject* registered = Register(std::move(newObj), parent);
	LOG("[Scene] Empty GameObject '%s' creado (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::CreatePrimitive(PrimitiveType type, GameObject* parent)
{
	std::string baseName = GeometryGenerator::GetName(type);
	std::string uniqueName = MakeUniqueName(baseName);
	auto newObj = std::make_unique<GameObject>(uniqueName);

	auto trans = std::make_unique<TransformComponent>(newObj.get());
	trans->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	newObj->AddComponent(std::move(trans));

	// Las primitivas son mallas internas del ResourceManager: todas las instancias comparten una
	newObj->AddComponent(std::make_unique<MeshComponent>(newObj.get(), ResourceManager::GetBuiltInMeshUID(type)));

	auto mat = std::make_unique<MaterialComponent>(newObj.get());
	mat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	mat->SetUseTexture(false);
	newObj->AddComponent(std::move(mat));

	GameObject* registered = Register(std::move(newObj), parent);
	LOG("[Scene] Primitiva '%s' creada (Total entidades: %zu)", uniqueName.c_str(), sceneEntities.size());
	return registered;
}

GameObject* Scene::ImportModel(const std::string& filePath, GameObject* parent)
{
	// Se copia a Assets si viene de fuera, se importa a Library y se instancia
	ResourceManager* resources = Engine::GetInstance().resources.get();
	if (!resources) return nullptr;
	const std::string assetPath = resources->ImportExternalFile(filePath, resources->GetImportFolder());
	const UID modelUID = assetPath.empty() ? INVALID_UID : resources->GetAssetUID(assetPath);
	return modelUID != INVALID_UID ? InstantiateModel(modelUID, parent) : nullptr;
}

GameObject* Scene::InstantiateModel(UID modelUID, GameObject* parent)
{
	ResourceManager* resources = Engine::GetInstance().resources.get();
	if (!resources) return nullptr;

	// El modelo solo se pide mientras se instancia; las mallas y texturas las piden los componentes
	Resource* resource = resources->RequestResource(modelUID);
	if (resource == nullptr || resource->GetType() != ResourceType::Model || !resource->IsLoaded())
	{
		if (resource) resources->ReleaseResource(modelUID);
		LOG_ERROR("[Scene] El modelo con UID %llu no existe o no se pudo cargar", (unsigned long long)modelUID);
		return nullptr;
	}

	const std::vector<ModelNode>& nodes = static_cast<const ResourceModel*>(resource)->GetNodes();
	std::vector<GameObject*> created(nodes.size(), nullptr);

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const ModelNode& node = nodes[i];
		auto go = std::make_unique<GameObject>(i == 0 ? MakeUniqueName(node.name) : node.name);

		auto transform = std::make_unique<TransformComponent>(go.get());
		transform->SetPosition(node.position);
		transform->SetRotation(node.rotation);
		transform->SetScale(node.scale);
		go->AddComponent(std::move(transform));

		if (node.mesh != INVALID_UID)
		{
			go->AddComponent(std::make_unique<MeshComponent>(go.get(), node.mesh));

			auto material = std::make_unique<MaterialComponent>(go.get());
			material->SetColor(node.color);
			material->SetTexture(node.texture);
			material->SetUseTexture(node.texture != INVALID_UID);
			go->AddComponent(std::move(material));
		}

		if (node.isCamera)
		{
			go->AddComponent(std::make_unique<CameraComponent>(go.get()));
		}

		// Los nodos vienen en preorden: el padre de 'i' ya se ha creado
		GameObject* nodeParent = (node.parent >= 0 && node.parent < static_cast<int>(i)) ? created[node.parent] : parent;
		created[i] = Register(std::move(go), nodeParent);
	}

	GameObject* root = created.empty() ? nullptr : created[0];
	if (root) LOG("[Scene] Modelo '%s' instanciado (%zu GameObjects)", resource->GetName().c_str(), created.size());

	resources->ReleaseResource(modelUID);
	spacePartitioningDirty = true;
	return root;
}

void Scene::OnResourcesRemoved(const std::vector<UID>& uids)
{
	if (uids.empty() || sceneEntities.empty()) return;

	const std::unordered_set<UID> removed(uids.begin(), uids.end());
	size_t affected = 0;
	for (const auto& entity : sceneEntities)
	{
		if (MeshComponent* mesh = entity->GetComponent<MeshComponent>())
		{
			if (mesh->GetMeshUID() != INVALID_UID && removed.count(mesh->GetMeshUID()) > 0)
			{
				mesh->MarkMeshMissing();
				++affected;
			}
		}
		if (MaterialComponent* material = entity->GetComponent<MaterialComponent>())
		{
			if (material->GetTextureUID() != INVALID_UID && removed.count(material->GetTextureUID()) > 0)
			{
				material->MarkTextureMissing();
				++affected;
			}
		}
	}

	if (affected > 0) LOG_WARNING("[Scene] %zu componente(s) usaban recursos borrados y se han quedado sin ellos", affected);
}

void Scene::DestroyGameObject(GameObject* target)
{
	
	if (target == nullptr || target == &sceneRoot) return;

	// Verificar pertenencia real a la escena
	if (!OwnsObject(target)) return;

	std::vector<GameObject*> toDestroy;
	std::stack<GameObject*> stack;
	stack.push(target);

	while (!stack.empty())
	{
		GameObject* node = stack.top();
		stack.pop();
		if (!node) continue;

		toDestroy.push_back(node);
		for (GameObject* child : node->GetChildren())
		{
			stack.push(child);
		}
	}
	std::reverse(toDestroy.begin(), toDestroy.end());

	for (GameObject* obj : toDestroy)
	{
		if (selected == obj)
		{
			SetSelected(nullptr);
			break;
		}
	}

	// Si la cámara principal estuviera en el subárbol, desvincularla para no dejar puntero colgante
	for (GameObject* obj : toDestroy)
	{
		if (mainCameraPtr == obj) mainCameraPtr = nullptr;
		if (gameCameraPtr == obj) gameCameraPtr = nullptr;
	}

	// Desvinculación inmediata del padre superior
	if (target->GetParent())
	{
		target->GetParent()->RemoveChild(target);
	}

	for (GameObject* obj : toDestroy)
	{
		if (std::find(pendingDestruction.begin(), pendingDestruction.end(), obj) == pendingDestruction.end())
		{
			pendingDestruction.push_back(obj);
		}
	}

	LOG("[Scene] GameObject '%s' y %zu descendiente(s) marcados para eliminación segura",
		target->GetName().c_str(), toDestroy.size() - 1);
	MarkSceneDirty();
}

void Scene::ProcessPendingDestructions()
{
	if (pendingDestruction.empty()) return;

	std::unordered_set<GameObject*> destroySet(pendingDestruction.begin(), pendingDestruction.end());
	pendingDestruction.clear();

	// Desvincular relaciones padre-hijo internas antes de la destrucción de memoria
	for (GameObject* obj : destroySet)
	{
		if (obj->GetParent() && destroySet.find(obj->GetParent()) == destroySet.end())
		{
			obj->GetParent()->RemoveChild(obj);
		}
	}

	const size_t initialCount = sceneEntities.size();
	sceneEntities.erase(
		std::remove_if(sceneEntities.begin(), sceneEntities.end(), [&](const std::unique_ptr<GameObject>& ptr)
		{
			return destroySet.find(ptr.get()) != destroySet.end();
		}),
		sceneEntities.end()
	);

	LOG("[Scene] Liberados %zu GameObjects de la memoria de la escena (%zu restantes)",
		initialCount - sceneEntities.size(), sceneEntities.size());
	spacePartitioningDirty = true;
}

GameObject* Scene::DuplicateGameObject(GameObject* target, GameObject* parent)
{
	if (target == nullptr || !OwnsObject(target))
	{
		LOG_WARNING("[Scene] No se puede duplicar: GameObject nulo o no pertenece a la escena.");
		return nullptr;
	}

	GameObject* targetParent = (parent != nullptr) ? parent : target->GetParent();
	if (targetParent == nullptr)
	{
		targetParent = &sceneRoot;
	}

	struct DuplicateTask
	{
		GameObject* src;
		GameObject* parentNode;
	};

	std::stack<DuplicateTask> stack;
	stack.push({ target, targetParent });
	GameObject* duplicatedRoot = nullptr;

	while (!stack.empty())
	{
		DuplicateTask current = stack.top();
		stack.pop();

		GameObject* src = current.src;
		GameObject* parentNode = current.parentNode;
		if (!src) continue;

		TransformComponent* srcTrans = src->GetComponent<TransformComponent>();
		glm::vec3 localPos = srcTrans ? srcTrans->GetPosition() : glm::vec3(0.0f);
		glm::vec3 localRot = srcTrans ? srcTrans->GetRotation() : glm::vec3(0.0f);
		glm::vec3 localScale = srcTrans ? srcTrans->GetScale() : glm::vec3(1.0f);

		std::unique_ptr<GameObject> clone = src->Duplicate();
		clone->SetName(MakeUniqueName(src->GetName()));
		GameObject* registeredClone = Register(std::move(clone), parentNode);

		if (TransformComponent* cloneTrans = registeredClone->GetComponent<TransformComponent>())
		{
			cloneTrans->SetPosition(localPos);
			cloneTrans->SetRotation(localRot);
			cloneTrans->SetScale(localScale);
		}

		if (duplicatedRoot == nullptr)
		{
			duplicatedRoot = registeredClone;
		}

		const std::vector<GameObject*>& children = src->GetChildren();
		for (size_t i = children.size(); i > 0; --i)
		{
			stack.push({ children[i - 1], registeredClone });
		}
	}

	SetSelected(duplicatedRoot);
	if (duplicatedRoot)
	{
		LOG("[Scene] GameObject '%s' duplicado como '%s' (Total entidades: %zu)",
			target->GetName().c_str(), duplicatedRoot->GetName().c_str(), sceneEntities.size());
	}

	return duplicatedRoot;
}

void Scene::HandleDroppedFile(const std::string& filePath)
{
	ImporterManager::HandleDroppedFile(filePath, selected);
	spacePartitioningDirty = true;
}

void Scene::SetTreeParameters(int maxObjectsPerNode, int maxDepth)
{
	maxObjectsPerNode = std::max(1, maxObjectsPerNode);
	maxDepth = std::max(1, maxDepth);
	if (treeMaxObjectsPerNode != maxObjectsPerNode || treeMaxDepth != maxDepth)
	{
		treeMaxObjectsPerNode = maxObjectsPerNode;
		treeMaxDepth = maxDepth;
		RebuildSpacePartitioning();
		MarkSceneDirty();
	}
}

void Scene::RebuildSpacePartitioning()
{
	std::vector<Quadtree::Entry> staticEntries;
	AABB worldBounds(glm::vec3(-8.0f, -2.0f, -8.0f), glm::vec3(8.0f, 8.0f, 8.0f));
	bool firstBox = true;

	std::stack<GameObject*> stack;
	stack.push(&sceneRoot);

	while (!stack.empty())
	{
		GameObject* node = stack.top();
		stack.pop();
		if (!node || !node->IsActive()) continue;

		if (node != &sceneRoot && node->IsStatic() && node->HasWorldAABB())
		{
			AABB box = node->GetWorldAABB();
			staticEntries.push_back({ node, box });

			if (firstBox)
			{
				worldBounds = box;
				firstBox = false;
			}
			else
			{
				worldBounds.Enclose(box);
			}
		}

		const std::vector<GameObject*>& children = node->GetChildren();
		for (size_t i = children.size(); i > 0; --i)
		{
			stack.push(children[i - 1]);
		}
	}

	// Asegurar un volumen mínimo simétrico y un pequeño margen alrededor de los objetos estáticos
	worldBounds.Enclose(AABB(glm::vec3(-6.0f, -1.0f, -6.0f), glm::vec3(6.0f, 6.0f, 6.0f)));
	worldBounds.min -= glm::vec3(0.5f);
	worldBounds.max += glm::vec3(0.5f);

	quadtree.Build(worldBounds, staticEntries, treeMaxObjectsPerNode, treeMaxDepth);
	octree.Build(worldBounds, staticEntries, treeMaxObjectsPerNode, treeMaxDepth);
	spacePartitioningDirty = false;

	LOG("[Scene] Space Partitioning reconstruido (%zu objetos Static): Quadtree=%zu nodos, Octree=%zu nodos",
		staticEntries.size(), quadtree.GetNodeCount(), octree.GetNodeCount());
}

void Scene::SetCurrentScenePath(const std::string& path)
{
	currentScenePath = path;
	UpdateWindowTitle();
}

std::string Scene::GetSceneDisplayName() const
{
	if (currentScenePath.empty())
	{
		return "Untitled";
	}
	std::filesystem::path p(currentScenePath);
	std::string stem = p.stem().string();
	return stem.empty() ? "Untitled" : stem;
}

void Scene::MarkSceneDirty()
{
	if (isLoadingOrInitializing) return;
	if (!sceneDirty)
	{
		sceneDirty = true;
		UpdateWindowTitle();
	}
}

void Scene::ClearSceneDirty()
{
	if (sceneDirty)
	{
		sceneDirty = false;
		UpdateWindowTitle();
	}
}

void Scene::UpdateWindowTitle() const
{
	if (Engine::GetInstance().window)
	{
		std::string suffix = " - " + GetSceneDisplayName();
		if (sceneDirty)
		{
			suffix += "*";
		}
		Engine::GetInstance().window->SetSceneTitleInfo(suffix);
	}
}

void Scene::NewScene()
{
	isLoadingOrInitializing = true;
	pendingDestruction.clear();
	selected = nullptr;
	gameCameraPtr = nullptr;

	while (!sceneRoot.GetChildren().empty())
	{
		sceneRoot.RemoveChild(sceneRoot.GetChildren().back());
	}
	sceneEntities.clear();

	// Instanciar Main Camera por defecto en la nueva escena
	auto gameCamObj = std::make_unique<GameObject>("Main Camera");
	auto gameCamTrans = std::make_unique<TransformComponent>(gameCamObj.get());
	gameCamTrans->SetPosition(glm::vec3(0.0f, 2.0f, 7.0f));
	gameCamTrans->SetRotation(glm::vec3(-10.0f, 0.0f, 0.0f));
	gameCamObj->AddComponent(std::move(gameCamTrans));

	auto gameCamComp = std::make_unique<CameraComponent>(gameCamObj.get());
	gameCamObj->AddComponent(std::move(gameCamComp));

	gameCameraPtr = Register(std::move(gameCamObj), &sceneRoot);
	selectionRevision++;
	RebuildSpacePartitioning();

	currentScenePath.clear();
	sceneDirty = false;
	isLoadingOrInitializing = false;
	UpdateWindowTitle();

	LOG("[Scene] Nueva escena creada ('Untitled', 1 entidad: 'Main Camera').");
}

// ============================================================================
// QUADTREE (Iterativo con std::stack)
// ============================================================================
void Quadtree::Clear()
{
	if (!root)
	{
		totalNodes = 0;
		totalObjects = 0;
		return;
	}

	std::stack<std::unique_ptr<Node>> cleanupStack;
	cleanupStack.push(std::move(root));

	while (!cleanupStack.empty())
	{
		std::unique_ptr<Node> current = std::move(cleanupStack.top());
		cleanupStack.pop();
		if (!current) continue;

		for (size_t i = 0; i < 4; ++i)
		{
			if (current->children[i])
			{
				cleanupStack.push(std::move(current->children[i]));
			}
		}
	}

	totalNodes = 0;
	totalObjects = 0;
}

void Quadtree::SubdivideNode(Node* node)
{
	if (!node || !node->IsLeaf()) return;

	const glm::vec3 minP = node->bounds.min;
	const glm::vec3 maxP = node->bounds.max;
	const glm::vec3 center = node->bounds.GetCenter();
	const int nextDepth = node->depth + 1;

	node->children[0] = std::make_unique<Node>();
	node->children[0]->bounds = AABB(glm::vec3(minP.x, minP.y, minP.z), glm::vec3(center.x, maxP.y, center.z));
	node->children[0]->depth = nextDepth;

	node->children[1] = std::make_unique<Node>();
	node->children[1]->bounds = AABB(glm::vec3(center.x, minP.y, minP.z), glm::vec3(maxP.x, maxP.y, center.z));
	node->children[1]->depth = nextDepth;

	node->children[2] = std::make_unique<Node>();
	node->children[2]->bounds = AABB(glm::vec3(minP.x, minP.y, center.z), glm::vec3(center.x, maxP.y, maxP.z));
	node->children[2]->depth = nextDepth;

	node->children[3] = std::make_unique<Node>();
	node->children[3]->bounds = AABB(glm::vec3(center.x, minP.y, center.z), glm::vec3(maxP.x, maxP.y, maxP.z));
	node->children[3]->depth = nextDepth;

	totalNodes += 4;
}

void Quadtree::Build(const AABB& worldBounds, const std::vector<Entry>& staticEntries, int maxObjectsPerNode, int maxDepth)
{
	Clear();

	maxObjects = std::max(1, maxObjectsPerNode);
	maxTreeDepth = std::max(1, maxDepth);
	totalObjects = staticEntries.size();

	root = std::make_unique<Node>();
	root->bounds = worldBounds;
	root->depth = 0;
	root->objects = staticEntries;
	totalNodes = 1;

	std::stack<Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		Node* current = stack.top();
		stack.pop();
		if (!current) continue;

		if ((int)current->objects.size() > maxObjects && current->depth < maxTreeDepth)
		{
			SubdivideNode(current);

			std::vector<Entry> currentEntries = std::move(current->objects);
			current->objects.clear();

			for (const Entry& entry : currentEntries)
			{
				for (size_t c = 0; c < 4; ++c)
				{
					if (current->children[c]->bounds.Intersects2DXZ(entry.box))
					{
						current->children[c]->objects.push_back(entry);
					}
				}
			}

			for (size_t c = 4; c > 0; --c)
			{
				Node* child = current->children[c - 1].get();
				if ((int)child->objects.size() > maxObjects && child->objects.size() < currentEntries.size())
				{
					stack.push(child);
				}
			}
		}
	}
}

int Quadtree::QueryFrustum(const Frustum& frustum, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		if (!frustum.IsVisible(node->bounds))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					if (frustum.IsVisible(entry.box))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 4; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

int Quadtree::QueryRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;
	const Ray ray(rayOrigin, rayDir);

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		float tHit = 0.0f;
		if (!Intersection::RayAABB(ray, node->bounds, tHit))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					float tObj = 0.0f;
					if (Intersection::RayAABB(ray, entry.box, tObj))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 4; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

void Quadtree::CollectNodeBounds(std::vector<AABB>& outBoxes) const
{
	outBoxes.clear();
	if (!root) return;

	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		outBoxes.push_back(node->bounds);

		if (!node->IsLeaf())
		{
			for (size_t i = 4; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}
}

// ============================================================================
// OCTREE (Iterativo con std::stack)
// ============================================================================
void Octree::Clear()
{
	if (!root)
	{
		totalNodes = 0;
		totalObjects = 0;
		return;
	}

	std::stack<std::unique_ptr<Node>> cleanupStack;
	cleanupStack.push(std::move(root));

	while (!cleanupStack.empty())
	{
		std::unique_ptr<Node> current = std::move(cleanupStack.top());
		cleanupStack.pop();
		if (!current) continue;

		for (size_t i = 0; i < 8; ++i)
		{
			if (current->children[i])
			{
				cleanupStack.push(std::move(current->children[i]));
			}
		}
	}

	totalNodes = 0;
	totalObjects = 0;
}

void Octree::SubdivideNode(Node* node)
{
	if (!node || !node->IsLeaf()) return;

	const glm::vec3 minP = node->bounds.min;
	const glm::vec3 maxP = node->bounds.max;
	const glm::vec3 center = node->bounds.GetCenter();
	const int nextDepth = node->depth + 1;

	for (size_t i = 0; i < 8; ++i)
	{
		glm::vec3 childMin(
			(i & 1) ? center.x : minP.x,
			(i & 2) ? center.y : minP.y,
			(i & 4) ? center.z : minP.z
		);
		glm::vec3 childMax(
			(i & 1) ? maxP.x : center.x,
			(i & 2) ? maxP.y : center.y,
			(i & 4) ? maxP.z : center.z
		);

		node->children[i] = std::make_unique<Node>();
		node->children[i]->bounds = AABB(childMin, childMax);
		node->children[i]->depth = nextDepth;
	}

	totalNodes += 8;
}

void Octree::Build(const AABB& worldBounds, const std::vector<Entry>& staticEntries, int maxObjectsPerNode, int maxDepth)
{
	Clear();

	maxObjects = std::max(1, maxObjectsPerNode);
	maxTreeDepth = std::max(1, maxDepth);
	totalObjects = staticEntries.size();

	root = std::make_unique<Node>();
	root->bounds = worldBounds;
	root->depth = 0;
	root->objects = staticEntries;
	totalNodes = 1;

	std::stack<Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		Node* current = stack.top();
		stack.pop();
		if (!current) continue;

		if ((int)current->objects.size() > maxObjects && current->depth < maxTreeDepth)
		{
			SubdivideNode(current);

			std::vector<Entry> currentEntries = std::move(current->objects);
			current->objects.clear();

			for (const Entry& entry : currentEntries)
			{
				for (size_t c = 0; c < 8; ++c)
				{
					if (current->children[c]->bounds.Intersects(entry.box))
					{
						current->children[c]->objects.push_back(entry);
					}
				}
			}

			for (size_t c = 8; c > 0; --c)
			{
				Node* child = current->children[c - 1].get();
				if ((int)child->objects.size() > maxObjects && child->objects.size() < currentEntries.size())
				{
					stack.push(child);
				}
			}
		}
	}
}

int Octree::QueryFrustum(const Frustum& frustum, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		if (!frustum.IsVisible(node->bounds))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					if (frustum.IsVisible(entry.box))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 8; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

int Octree::QueryRay(const glm::vec3& rayOrigin, const glm::vec3& rayDir, std::vector<GameObject*>& outObjects) const
{
	outObjects.clear();
	if (!root) return 0;
	const Ray ray(rayOrigin, rayDir);

	int visitedNodes = 0;
	std::unordered_set<GameObject*> uniqueSet;
	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		visitedNodes++;
		float tHit = 0.0f;
		if (!Intersection::RayAABB(ray, node->bounds, tHit))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					float tObj = 0.0f;
					if (Intersection::RayAABB(ray, entry.box, tObj))
					{
						uniqueSet.insert(entry.object);
						outObjects.push_back(entry.object);
					}
				}
			}
		}
		else
		{
			for (size_t i = 8; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}

	return visitedNodes;
}

void Octree::CollectNodeBounds(std::vector<AABB>& outBoxes) const
{
	outBoxes.clear();
	if (!root) return;

	std::stack<const Node*> stack;
	stack.push(root.get());

	while (!stack.empty())
	{
		const Node* node = stack.top();
		stack.pop();
		if (!node) continue;

		outBoxes.push_back(node->bounds);

		if (!node->IsLeaf())
		{
			for (size_t i = 8; i > 0; --i)
			{
				if (node->children[i - 1])
				{
					stack.push(node->children[i - 1].get());
				}
			}
		}
	}
}
