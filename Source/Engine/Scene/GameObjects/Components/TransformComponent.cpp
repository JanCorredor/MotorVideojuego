#include "TransformComponent.h"
#include "../GameObject.h"

const glm::mat4& TransformComponent::GetWorldMatrix() const
{
	if (worldDirty)
	{
		const GameObject* parent = owner ? owner->GetParent() : nullptr;
		worldMatrix = parent ? parent->GetWorldMatrix() * GetLocalModelMatrix() : GetLocalModelMatrix();
		worldDirty = false;
	}
	return worldMatrix;
}

void TransformComponent::InvalidateWorld()
{
	// Si ya estaba sucio, por la invariante sus descendientes también lo están
	if (worldDirty) return;
	worldDirty = true;

	if (owner == nullptr) return;
	for (GameObject* child : owner->GetChildren())
	{
		child->InvalidateWorldTransform();
	}
}
