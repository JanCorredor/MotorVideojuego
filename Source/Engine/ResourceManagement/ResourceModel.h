#pragma once

#include "Resource.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

// Un nodo de la jerarquia de un modelo importado (un futuro GameObject)
struct ModelNode
{
	std::string name;
	int parent = -1;                        // indice del padre en el vector de nodos (-1 = raiz)
	glm::vec3 position = glm::vec3(0.0f);   // transform LOCAL respecto al padre
	glm::vec3 rotation = glm::vec3(0.0f);   // angulos de Euler XYZ en grados
	glm::vec3 scale = glm::vec3(1.0f);
	UID mesh = INVALID_UID;                 // malla del nodo (0 = nodo vacio)
	UID texture = INVALID_UID;              // textura difusa del material (0 = sin textura)
	glm::vec4 color = glm::vec4(1.0f);      // color del material
	bool isCamera = false;                  // el nodo era una camara en el fichero original
};

// Receta de un modelo importado (Library/Models/<UID>.model, JSON): jerarquía de nodos con sus
// transforms y los UID de malla y textura de cada uno. La escena la usa para instanciarlo
class ResourceModel : public Resource
{
public:
	explicit ResourceModel(UID uid) : Resource(uid, ResourceType::Model) {}
	~ResourceModel() override = default;

	const std::vector<ModelNode>& GetNodes() const { return nodes; }

	size_t GetMemoryUsage() const override { return nodes.size() * sizeof(ModelNode); }

protected:
	bool LoadInMemory() override;
	void UnloadFromMemory() override;

private:
	std::vector<ModelNode> nodes;
};
