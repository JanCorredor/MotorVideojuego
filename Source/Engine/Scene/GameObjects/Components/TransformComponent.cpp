#include "TransformComponent.h"
#include "../GameObject.h"

std::unique_ptr<Component> TransformComponent::Clone(GameObject* newOwner) const
{
	auto clone = std::make_unique<TransformComponent>(newOwner);
	clone->SetPosition(position);
	clone->SetRotation(rotation);
	clone->SetScale(scale);
	if (!active) clone->Disable();
	return clone;
}

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
