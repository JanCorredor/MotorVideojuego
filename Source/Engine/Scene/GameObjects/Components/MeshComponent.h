#pragma once

#include "Component.h"
#include "../../../Render/Primitives/Primitive.h"
#include <memory>

class MeshComponent : public Component
{
public:
	MeshComponent(GameObject* owner, std::unique_ptr<Primitive> prim = nullptr) 
		: Component(COMPONENT_MESH, owner), primitive(std::move(prim)) {}
	virtual ~MeshComponent() {}

	Primitive* GetPrimitive() const { return primitive.get(); }
	void SetPrimitive(std::unique_ptr<Primitive> prim) { primitive = std::move(prim); }

private:
	std::unique_ptr<Primitive> primitive;
};
