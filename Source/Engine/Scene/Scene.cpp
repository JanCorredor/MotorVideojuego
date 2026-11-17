#include "../Engine.h"
#include "../Render/Render.h"
#include "../Render/3DModels/ModelLoader.h"
#include "../Input/Input.h"
#include "../../Utils/Log.h"
#include "Scene.h"
#include "GameObjects/Components/TransformComponent.h"
#include "GameObjects/Components/MeshComponent.h"
#include "GameObjects/Components/MaterialComponent.h"
#include "GameObjects/Components/CameraComponent.h"
#include "../Render/Primitives/GeometryGenerator.h"

#include "../ResourceManagement/Importers/ImporterManager.h"
#include "../ResourceManagement/Importers/MeshImporter.h"
#include "../ResourceManagement/Importers/TextureImporter.h"

#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stack>
#include <cfloat>
#include <cmath>

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

	// Create Cube
	auto cubeObj = std::make_unique<GameObject>("Cube");
	auto cubeTrans = std::make_unique<TransformComponent>(cubeObj.get());
	cubeTrans->SetPosition(glm::vec3(-2.0f, 0.0f, 0.0f));
	cubeObj->AddComponent(std::move(cubeTrans));

	auto cubeMesh = std::make_unique<MeshComponent>(cubeObj.get(), GeometryGenerator::CreateCube(1.0f));
	cubeObj->AddComponent(std::move(cubeMesh));

	auto cubeMat = std::make_unique<MaterialComponent>(cubeObj.get());
	cubeMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	cubeMat->SetTexture("../Assets/water.jpg");
	cubeMat->SetUseTexture(true);
	cubeObj->AddComponent(std::move(cubeMat));

	// Create Sphere
	auto sphereObj = std::make_unique<GameObject>("Sphere");
	auto sphereTrans = std::make_unique<TransformComponent>(sphereObj.get());
	sphereTrans->SetPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	sphereObj->AddComponent(std::move(sphereTrans));

	auto sphereMesh = std::make_unique<MeshComponent>(sphereObj.get(), GeometryGenerator::CreateSphere(0.5f, 20, 20));
	sphereObj->AddComponent(std::move(sphereMesh));

	auto sphereMat = std::make_unique<MaterialComponent>(sphereObj.get());
	sphereMat->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	sphereMat->SetTexture("../Assets/lava.jpg");
	sphereMat->SetUseTexture(true);
	sphereObj->AddComponent(std::move(sphereMat));

	cubeObj->SetStatic(true);
	sphereObj->SetStatic(true);

	cubeObj->AddChild(sphereObj.get());
	sceneEntities.push_back(std::move(sphereObj));


	sceneRoot.AddChild(cubeObj.get());
	sceneEntities.push_back(std::move(cubeObj));
	LOG("[Scene] Instanciado 'Cube' con geometría procedural de cubo");


	LOG("[Scene] Instanciado 'Sphere' con geometría procedural de esfera y textura");

	// MODELO CASA CON TEXTURA PNG (Soporta múltiples partes)
	GameObject* bakerHouse = ImportModel("../Assets/BakerHouse.fbx", &sceneRoot);
	if (bakerHouse)
	{
		TextureImporter::ImportAndAssignToGameObject("../Assets/Baker_house.png", bakerHouse);
		if (TransformComponent* bakerHouseTransform = bakerHouse->GetComponent<TransformComponent>())
		{
			bakerHouseTransform->SetPosition(glm::vec3(0.0f, 0.0f, -3.0f));
		}
		bakerHouse->SetStatic(true, true);
	}

	// MODELO CON TEXTURA TGA 
	GameObject* candle = ImportModel("../Assets/SM_Candle_Fundida.fbx", &sceneRoot);
	if (candle)
	{
		TextureImporter::ImportAndAssignToGameObject("../Assets/T_Candle_D.tga", candle);
		if (TransformComponent* candleTransform = candle->GetComponent<TransformComponent>())
		{
			candleTransform->SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
			candleTransform->SetRotation(glm::vec3(-90.0f, 0.0f, 0.0f));
			candleTransform->SetScale(glm::vec3(0.05f, 0.05f, 0.05f));
		}
		candle->SetStatic(true, true);
	}

	// MODELO SIN TEXTURA
	GameObject* character = ImportModel("../Assets/HeavyTF2_2018.fbx", &sceneRoot);
	if (character)
	{
		TransformComponent* characterTransform = character->GetComponent<TransformComponent>();
		if (characterTransform)
		{
			characterTransform->SetPosition(glm::vec3(-3.0f, 0.0f, -2.0f));
			characterTransform->SetScale(glm::vec3(0.02f, 0.02f, 0.02f));
		}
		character->SetStatic(true, true);
	}

	RebuildSpacePartitioning();
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
		selected = nullptr;
		return;
	}
	if (OwnsObject(object))
	{
		selected = object;
	}
	else
	{
		selected = nullptr;
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
		auto newMesh = std::make_unique<MeshComponent>(newObj.get(), GeometryGenerator::CreateCube(1.0f));
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

	auto prim = GeometryGenerator::Create(type);
	auto mesh = std::make_unique<MeshComponent>(newObj.get(), std::move(prim));
	newObj->AddComponent(std::move(mesh));

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
	return MeshImporter::ImportToScene(filePath, parent);
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
	ImporterManager::ImportFile(filePath, selected);
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
				worldBounds.Encapsulate(box);
			}
		}

		const std::vector<GameObject*>& children = node->GetChildren();
		for (size_t i = children.size(); i > 0; --i)
		{
			stack.push(children[i - 1]);
		}
	}

	// Asegurar un volumen mínimo simétrico y un pequeño margen alrededor de los objetos estáticos
	worldBounds.Encapsulate(AABB(glm::vec3(-6.0f, -1.0f, -6.0f), glm::vec3(6.0f, 6.0f, 6.0f)));
	worldBounds.minPoint -= glm::vec3(0.5f);
	worldBounds.maxPoint += glm::vec3(0.5f);

	quadtree.Build(worldBounds, staticEntries, treeMaxObjectsPerNode, treeMaxDepth);
	octree.Build(worldBounds, staticEntries, treeMaxObjectsPerNode, treeMaxDepth);
	spacePartitioningDirty = false;

	LOG("[Scene] Space Partitioning reconstruido (%zu objetos Static): Quadtree=%zu nodos, Octree=%zu nodos",
		staticEntries.size(), quadtree.GetNodeCount(), octree.GetNodeCount());
}

bool Scene::SaveTemporaryScene(const std::string& filePath) const
{
	std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
	if (!out.is_open())
	{
		LOG_ERROR("[Scene] No se pudo crear el archivo temporal de escena: '%s'", filePath.c_str());
		return false;
	}

	auto writeString = [&out](const std::string& s)
	{
		uint32_t len = (uint32_t)s.size();
		out.write(reinterpret_cast<const char*>(&len), sizeof(len));
		if (len > 0)
		{
			out.write(s.data(), len);
		}
	};

	const char magic[8] = { 'M', 'V', 'S', 'C', 'E', 'N', 'E', '1' };
	out.write(magic, sizeof(magic));
	out.write(reinterpret_cast<const char*>(&treeMaxObjectsPerNode), sizeof(treeMaxObjectsPerNode));
	out.write(reinterpret_cast<const char*>(&treeMaxDepth), sizeof(treeMaxDepth));

	// Recolectar los GameObjects en pre-orden usando std::stack para que todo padre preceda a sus hijos
	std::vector<const GameObject*> orderedObjects;
	std::stack<const GameObject*> stack;
	const std::vector<GameObject*>& rootChildren = sceneRoot.GetChildren();
	for (size_t i = rootChildren.size(); i > 0; --i)
	{
		if (rootChildren[i - 1])
		{
			stack.push(rootChildren[i - 1]);
		}
	}

	while (!stack.empty())
	{
		const GameObject* current = stack.top();
		stack.pop();
		if (!current) continue;

		orderedObjects.push_back(current);

		const std::vector<GameObject*>& children = current->GetChildren();
		for (size_t i = children.size(); i > 0; --i)
		{
			if (children[i - 1])
			{
				stack.push(children[i - 1]);
			}
		}
	}

	uint32_t objectCount = (uint32_t)orderedObjects.size();
	out.write(reinterpret_cast<const char*>(&objectCount), sizeof(objectCount));

	for (size_t idx = 0; idx < orderedObjects.size(); ++idx)
	{
		const GameObject* go = orderedObjects[idx];
		uint64_t uuid = (uint64_t)go->GetUUID();
		uint64_t parentUuid = 0;
		if (go->GetParent() != nullptr && go->GetParent() != &sceneRoot)
		{
			parentUuid = (uint64_t)go->GetParent()->GetUUID();
		}

		out.write(reinterpret_cast<const char*>(&uuid), sizeof(uuid));
		out.write(reinterpret_cast<const char*>(&parentUuid), sizeof(parentUuid));
		writeString(go->GetName());

		uint8_t flags = 0;
		if (go->IsActive())         flags |= (1 << 0);
		if (go->IsStatic())         flags |= (1 << 1);
		if (go == gameCameraPtr)    flags |= (1 << 2);
		if (go == selected)         flags |= (1 << 3);
		out.write(reinterpret_cast<const char*>(&flags), sizeof(flags));

		const std::array<ComponentType, COMPONENT_COUNT>& order = go->GetComponentOrder();
		for (size_t c = 0; c < COMPONENT_COUNT; ++c)
		{
			int32_t cType = (int32_t)order[c];
			out.write(reinterpret_cast<const char*>(&cType), sizeof(cType));
		}

		// 1. TransformComponent
		const TransformComponent* trans = go->GetComponent<TransformComponent>();
		uint8_t hasTrans = (trans != nullptr) ? 1 : 0;
		out.write(reinterpret_cast<const char*>(&hasTrans), sizeof(hasTrans));
		if (trans)
		{
			uint8_t cActive = trans->IsActive() ? 1 : 0;
			glm::vec3 pos = trans->GetPosition();
			glm::vec3 rot = trans->GetRotation();
			glm::vec3 scl = trans->GetScale();
			out.write(reinterpret_cast<const char*>(&cActive), sizeof(cActive));
			out.write(reinterpret_cast<const char*>(&pos), sizeof(pos));
			out.write(reinterpret_cast<const char*>(&rot), sizeof(rot));
			out.write(reinterpret_cast<const char*>(&scl), sizeof(scl));
		}

		// 2. MeshComponent
		const MeshComponent* mesh = go->GetComponent<MeshComponent>();
		uint8_t hasMesh = (mesh != nullptr) ? 1 : 0;
		out.write(reinterpret_cast<const char*>(&hasMesh), sizeof(hasMesh));
		if (mesh)
		{
			uint8_t cActive = mesh->IsActive() ? 1 : 0;
			out.write(reinterpret_cast<const char*>(&cActive), sizeof(cActive));
			const Primitive* prim = mesh->GetPrimitive();
			uint8_t hasPrim = (prim != nullptr) ? 1 : 0;
			out.write(reinterpret_cast<const char*>(&hasPrim), sizeof(hasPrim));
			if (prim)
			{
				const std::vector<Vertex>& verts = prim->GetVertices();
				const std::vector<unsigned int>& inds = prim->GetIndices();
				uint32_t vCount = (uint32_t)verts.size();
				uint32_t iCount = (uint32_t)inds.size();
				out.write(reinterpret_cast<const char*>(&vCount), sizeof(vCount));
				if (vCount > 0)
				{
					out.write(reinterpret_cast<const char*>(verts.data()), vCount * sizeof(Vertex));
				}
				out.write(reinterpret_cast<const char*>(&iCount), sizeof(iCount));
				if (iCount > 0)
				{
					out.write(reinterpret_cast<const char*>(inds.data()), iCount * sizeof(unsigned int));
				}
			}
		}

		// 3. MaterialComponent
		const MaterialComponent* mat = go->GetComponent<MaterialComponent>();
		uint8_t hasMat = (mat != nullptr) ? 1 : 0;
		out.write(reinterpret_cast<const char*>(&hasMat), sizeof(hasMat));
		if (mat)
		{
			uint8_t cActive = mat->IsActive() ? 1 : 0;
			glm::vec4 col = mat->GetColor();
			uint8_t useTex = mat->GetUseTexture() ? 1 : 0;
			uint8_t useCheck = mat->GetUseCheckerboard() ? 1 : 0;
			uint8_t useMissing = mat->GetUsingMissingTexture() ? 1 : 0;
			std::string texPath = (mat->GetTexture() != nullptr) ? mat->GetTexture()->path : "";

			out.write(reinterpret_cast<const char*>(&cActive), sizeof(cActive));
			out.write(reinterpret_cast<const char*>(&col), sizeof(col));
			out.write(reinterpret_cast<const char*>(&useTex), sizeof(useTex));
			out.write(reinterpret_cast<const char*>(&useCheck), sizeof(useCheck));
			out.write(reinterpret_cast<const char*>(&useMissing), sizeof(useMissing));
			writeString(texPath);
		}

		// 4. CameraComponent
		const CameraComponent* cam = go->GetComponent<CameraComponent>();
		uint8_t hasCam = (cam != nullptr) ? 1 : 0;
		out.write(reinterpret_cast<const char*>(&hasCam), sizeof(hasCam));
		if (cam)
		{
			uint8_t cActive = cam->IsActive() ? 1 : 0;
			float fov = cam->fov;
			float nearP = cam->nearPlane;
			float farP = cam->farPlane;
			float speed = cam->GetMovementSpeed();
			float sens = cam->GetMouseSensitivity();
			float orbit = cam->GetOrbitDistance();

			out.write(reinterpret_cast<const char*>(&cActive), sizeof(cActive));
			out.write(reinterpret_cast<const char*>(&fov), sizeof(fov));
			out.write(reinterpret_cast<const char*>(&nearP), sizeof(nearP));
			out.write(reinterpret_cast<const char*>(&farP), sizeof(farP));
			out.write(reinterpret_cast<const char*>(&speed), sizeof(speed));
			out.write(reinterpret_cast<const char*>(&sens), sizeof(sens));
			out.write(reinterpret_cast<const char*>(&orbit), sizeof(orbit));
		}
	}

	out.close();
	LOG("[Scene] Archivo temporal de escena guardado con éxito: '%s' (%u GameObjects)", filePath.c_str(), objectCount);
	return true;
}

bool Scene::LoadTemporaryScene(const std::string& filePath, bool deleteFileAfterLoad)
{
	std::ifstream in(filePath, std::ios::binary);
	if (!in.is_open())
	{
		LOG_ERROR("[Scene] No se pudo abrir el archivo temporal de escena: '%s'", filePath.c_str());
		return false;
	}

	auto readString = [&in]() -> std::string
	{
		uint32_t len = 0;
		in.read(reinterpret_cast<char*>(&len), sizeof(len));
		if (!in || len == 0) return "";
		std::string s(len, '\0');
		in.read(&s[0], len);
		return s;
	};

	char magic[8] = {};
	in.read(magic, sizeof(magic));
	const char expectedMagic[8] = { 'M', 'V', 'S', 'C', 'E', 'N', 'E', '1' };
	for (int i = 0; i < 8; ++i)
	{
		if (magic[i] != expectedMagic[i])
		{
			LOG_ERROR("[Scene] Formato inválido en archivo temporal de escena: '%s'", filePath.c_str());
			return false;
		}
	}

	in.read(reinterpret_cast<char*>(&treeMaxObjectsPerNode), sizeof(treeMaxObjectsPerNode));
	in.read(reinterpret_cast<char*>(&treeMaxDepth), sizeof(treeMaxDepth));

	uint32_t objectCount = 0;
	in.read(reinterpret_cast<char*>(&objectCount), sizeof(objectCount));

	// Limpiar escena actual antes de reconstruir
	pendingDestruction.clear();
	selected = nullptr;
	gameCameraPtr = nullptr;

	while (!sceneRoot.GetChildren().empty())
	{
		sceneRoot.RemoveChild(sceneRoot.GetChildren().back());
	}
	sceneEntities.clear();

	std::unordered_map<uint64_t, GameObject*> uuidMap;
	GameObject* restoredSelected = nullptr;

	for (uint32_t idx = 0; idx < objectCount; ++idx)
	{
		uint64_t uuid = 0;
		uint64_t parentUuid = 0;
		in.read(reinterpret_cast<char*>(&uuid), sizeof(uuid));
		in.read(reinterpret_cast<char*>(&parentUuid), sizeof(parentUuid));
		std::string objName = readString();

		uint8_t flags = 0;
		in.read(reinterpret_cast<char*>(&flags), sizeof(flags));

		std::array<ComponentType, COMPONENT_COUNT> savedOrder = {
			COMPONENT_TRANSFORM, COMPONENT_MESH, COMPONENT_MATERIAL, COMPONENT_CAMERA
		};
		for (size_t c = 0; c < COMPONENT_COUNT; ++c)
		{
			int32_t cType = 0;
			in.read(reinterpret_cast<char*>(&cType), sizeof(cType));
			if (cType >= 0 && cType < COMPONENT_COUNT)
			{
				savedOrder[c] = (ComponentType)cType;
			}
		}

		auto newObj = std::make_unique<GameObject>(objName, GameUUID(uuid));
		if (flags & (1 << 0)) newObj->Enable();
		else newObj->Disable();
		newObj->SetStatic((flags & (1 << 1)) != 0, false);

		GameObject* parentPtr = &sceneRoot;
		if (parentUuid != 0)
		{
			std::unordered_map<uint64_t, GameObject*>::iterator it = uuidMap.find(parentUuid);
			if (it != uuidMap.end() && it->second != nullptr)
			{
				parentPtr = it->second;
			}
		}

		// Registrar primero en el padre antes de asignar el Transform local para evitar recomposición de matriz
		GameObject* regObj = Register(std::move(newObj), parentPtr);
		uuidMap[uuid] = regObj;

		if (flags & (1 << 2)) gameCameraPtr = regObj;
		if (flags & (1 << 3)) restoredSelected = regObj;

		// 1. TransformComponent
		uint8_t hasTrans = 0;
		in.read(reinterpret_cast<char*>(&hasTrans), sizeof(hasTrans));
		if (hasTrans)
		{
			uint8_t cActive = 1;
			glm::vec3 pos(0.0f), rot(0.0f), scl(1.0f);
			in.read(reinterpret_cast<char*>(&cActive), sizeof(cActive));
			in.read(reinterpret_cast<char*>(&pos), sizeof(pos));
			in.read(reinterpret_cast<char*>(&rot), sizeof(rot));
			in.read(reinterpret_cast<char*>(&scl), sizeof(scl));

			auto trans = std::make_unique<TransformComponent>(regObj);
			trans->SetPosition(pos);
			trans->SetRotation(rot);
			trans->SetScale(scl);
			if (!cActive) trans->Disable();
			regObj->AddComponent(std::move(trans));
		}

		// 2. MeshComponent
		uint8_t hasMesh = 0;
		in.read(reinterpret_cast<char*>(&hasMesh), sizeof(hasMesh));
		if (hasMesh)
		{
			uint8_t cActive = 1;
			uint8_t hasPrim = 0;
			in.read(reinterpret_cast<char*>(&cActive), sizeof(cActive));
			in.read(reinterpret_cast<char*>(&hasPrim), sizeof(hasPrim));

			std::unique_ptr<Primitive> prim = nullptr;
			if (hasPrim)
			{
				uint32_t vCount = 0;
				uint32_t iCount = 0;
				in.read(reinterpret_cast<char*>(&vCount), sizeof(vCount));
				std::vector<Vertex> verts(vCount);
				if (vCount > 0)
				{
					in.read(reinterpret_cast<char*>(verts.data()), vCount * sizeof(Vertex));
				}
				in.read(reinterpret_cast<char*>(&iCount), sizeof(iCount));
				std::vector<unsigned int> inds(iCount);
				if (iCount > 0)
				{
					in.read(reinterpret_cast<char*>(inds.data()), iCount * sizeof(unsigned int));
				}
				prim = std::make_unique<Primitive>();
				prim->Init(std::move(verts), std::move(inds));
			}

			auto mesh = std::make_unique<MeshComponent>(regObj, std::move(prim));
			if (!cActive) mesh->Disable();
			regObj->AddComponent(std::move(mesh));
		}

		// 3. MaterialComponent
		uint8_t hasMat = 0;
		in.read(reinterpret_cast<char*>(&hasMat), sizeof(hasMat));
		if (hasMat)
		{
			uint8_t cActive = 1;
			glm::vec4 col(1.0f);
			uint8_t useTex = 0, useCheck = 0, useMissing = 0;
			in.read(reinterpret_cast<char*>(&cActive), sizeof(cActive));
			in.read(reinterpret_cast<char*>(&col), sizeof(col));
			in.read(reinterpret_cast<char*>(&useTex), sizeof(useTex));
			in.read(reinterpret_cast<char*>(&useCheck), sizeof(useCheck));
			in.read(reinterpret_cast<char*>(&useMissing), sizeof(useMissing));
			std::string texPath = readString();

			auto mat = std::make_unique<MaterialComponent>(regObj);
			mat->SetColor(col);
			if (!texPath.empty() && texPath.find("(procedural)") == std::string::npos)
			{
				mat->SetTexture(texPath);
			}
			mat->SetUseTexture(useTex != 0);
			mat->SetUseCheckerboard(useCheck != 0);
			mat->SetUsingMissingTexture(useMissing != 0);
			if (!cActive) mat->Disable();
			regObj->AddComponent(std::move(mat));
		}

		// 4. CameraComponent
		uint8_t hasCam = 0;
		in.read(reinterpret_cast<char*>(&hasCam), sizeof(hasCam));
		if (hasCam)
		{
			uint8_t cActive = 1;
			float fov = 45.0f, nearP = 0.1f, farP = 100.0f, speed = 5.0f, sens = 0.1f, orbit = 5.0f;
			in.read(reinterpret_cast<char*>(&cActive), sizeof(cActive));
			in.read(reinterpret_cast<char*>(&fov), sizeof(fov));
			in.read(reinterpret_cast<char*>(&nearP), sizeof(nearP));
			in.read(reinterpret_cast<char*>(&farP), sizeof(farP));
			in.read(reinterpret_cast<char*>(&speed), sizeof(speed));
			in.read(reinterpret_cast<char*>(&sens), sizeof(sens));
			in.read(reinterpret_cast<char*>(&orbit), sizeof(orbit));

			auto cam = std::make_unique<CameraComponent>(regObj);
			cam->fov = fov;
			cam->nearPlane = nearP;
			cam->farPlane = farP;
			cam->SetMovementSpeed(speed);
			cam->SetMouseSensitivity(sens);
			cam->SetOrbitDistance(orbit);
			if (!cActive) cam->Disable();
			regObj->AddComponent(std::move(cam));
		}

		regObj->SetComponentOrder(savedOrder);
	}

	in.close();

	if (deleteFileAfterLoad)
	{
		std::error_code ec;
		std::filesystem::remove(filePath, ec);
		LOG("[Scene] Archivo temporal '%s' restaurado (%u objetos) y eliminado del disco", filePath.c_str(), objectCount);
	}
	else
	{
		LOG("[Scene] Archivo temporal '%s' restaurado (%u objetos)", filePath.c_str(), objectCount);
	}

	selected = restoredSelected;
	RebuildSpacePartitioning();
	return true;
}

// ============================================================================
// AABB
// ============================================================================
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

// ============================================================================
// FrustumPlane & Frustum
// ============================================================================
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

	const glm::vec3 minP = node->bounds.minPoint;
	const glm::vec3 maxP = node->bounds.maxPoint;
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
		if (!frustum.IntersectsAABB(node->bounds))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					if (frustum.IntersectsAABB(entry.box))
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
		if (!node->bounds.IntersectsRay(rayOrigin, rayDir, tHit))
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
					if (entry.box.IntersectsRay(rayOrigin, rayDir, tObj))
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

	const glm::vec3 minP = node->bounds.minPoint;
	const glm::vec3 maxP = node->bounds.maxPoint;
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
		if (!frustum.IntersectsAABB(node->bounds))
		{
			continue;
		}

		if (node->IsLeaf())
		{
			for (const Entry& entry : node->objects)
			{
				if (entry.object && uniqueSet.find(entry.object) == uniqueSet.end())
				{
					if (frustum.IntersectsAABB(entry.box))
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
		if (!node->bounds.IntersectsRay(rayOrigin, rayDir, tHit))
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
					if (entry.box.IntersectsRay(rayOrigin, rayDir, tObj))
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
