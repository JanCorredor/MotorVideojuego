#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

class GameObject;

// Archivo / Nodo de serialización de escena en formato JSON.
// Permite que Scene::Save(file) pase el nodo 'file' a cada GameObject::Save(file),
// y que éste se lo pase a sus hijos y a sus Component::Save(file).
class SceneFile
{
public:
	SceneFile();
	~SceneFile();

	SceneFile(const SceneFile& other);
	SceneFile& operator=(const SceneFile& other);

	// Carga desde disco o guarda a disco en formato JSON legible (Pretty JSON)
	bool LoadFromFile(const std::string& filePath);
	bool SaveToFile(const std::string& filePath) const;

	bool IsValid() const;

	// Escritura de propiedades
	void SetBool(const char* key, bool value);
	void SetInt(const char* key, int value);
	void SetUInt64(const char* key, uint64_t value);
	void SetFloat(const char* key, float value);
	void SetString(const char* key, const std::string& value);
	void SetVec3(const char* key, const glm::vec3& value);
	void SetVec4(const char* key, const glm::vec4& value);

	// Lectura de propiedades con valor por defecto
	bool HasKey(const char* key) const;
	bool GetBool(const char* key, bool defaultVal = false) const;
	int GetInt(const char* key, int defaultVal = 0) const;
	uint64_t GetUInt64(const char* key, uint64_t defaultVal = 0) const;
	float GetFloat(const char* key, float defaultVal = 0.0f) const;
	std::string GetString(const char* key, const std::string& defaultVal = "") const;
	glm::vec3 GetVec3(const char* key, const glm::vec3& defaultVal = glm::vec3(0.0f)) const;
	glm::vec4 GetVec4(const char* key, const glm::vec4& defaultVal = glm::vec4(0.0f)) const;

	// Sub-objetos y Arrays para propagar Save(file) / Load(file) a hijos y componentes
	SceneFile AddObject(const char* key);
	SceneFile GetObjectNode(const char* key) const;

	SceneFile AddArray(const char* key);
	SceneFile GetArray(const char* key) const;

	SceneFile PushObject();
	size_t Size() const;
	SceneFile GetElement(size_t index) const;

	// Utilidades de rutas para Scenes y Library/Meshes (Custom File Format)
	static std::string ResolveAssetsDirectory();
	static std::string ResolveScenesDirectory();
	static std::string ResolveLibraryMeshesDirectory();
	static std::string ResolveExistingPath(const std::string& storedPath);
	static std::vector<std::string> ListSavedSceneFiles();

	struct Node;
private:
	std::shared_ptr<Node> node;
	explicit SceneFile(std::shared_ptr<Node> n);
};

enum ComponentType
{
	COMPONENT_TRANSFORM,
	COMPONENT_MESH,
	COMPONENT_MATERIAL,
	COMPONENT_CAMERA,
	COMPONENT_COUNT //Mantener al final
};

class Component
{
public:
	Component(ComponentType type, GameObject* owner) : type(type), owner(owner) {}
	virtual ~Component() {}

	virtual void Update(float dt) {}
    
    ComponentType GetType() const { return type; }
    GameObject* GetOwner() const { return owner; }
    
	bool IsActive() const { return active; }
	virtual void Enable() { if (!this->active) { this->active = true; NotifyOwnerModified(); } }
	virtual void Disable() { if (this->active) { this->active = false; NotifyOwnerModified(); } }

	void NotifyOwnerModified() const;

	virtual std::unique_ptr<Component> Clone(GameObject* newOwner) const = 0;

	// Serialización del Componente (Scene Serialization)
	virtual void Save(SceneFile& file) const = 0;
	virtual void Load(const SceneFile& file) = 0;

protected:
	ComponentType type;
	bool active = true;
	GameObject* owner = nullptr;
};
