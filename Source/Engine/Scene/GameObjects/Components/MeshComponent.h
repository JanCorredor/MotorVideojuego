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

	std::unique_ptr<Component> Clone(GameObject* newOwner) const override
	{
		std::unique_ptr<Primitive> primClone = primitive ? primitive->Clone() : nullptr;
		auto clone = std::make_unique<MeshComponent>(newOwner, std::move(primClone));
		if (!active) clone->Disable();
		return clone;
	}

private:
	std::unique_ptr<Primitive> primitive;
};
