#include "TransformComponent.h"
#include "../GameObject.h"
#include <stack>
#include <vector>

std::unique_ptr<Component> TransformComponent::Clone(GameObject* newOwner) const
{
	auto clone = std::make_unique<TransformComponent>(newOwner);
	clone->SetPosition(position);
	clone->SetRotation(rotation);
	clone->SetScale(scale);
	if (!active) clone->Disable();
	return clone;
}

void TransformComponent::Save(SceneFile& file) const
{
	file.SetBool("active", active);
	file.SetVec3("position", position);
	file.SetVec3("rotation", rotation);
	file.SetVec3("scale", scale);
}

void TransformComponent::Load(const SceneFile& file)
{
	active = file.GetBool("active", true);
	SetPosition(file.GetVec3("position", glm::vec3(0.0f)));
	SetRotation(file.GetVec3("rotation", glm::vec3(0.0f)));
	SetScale(file.GetVec3("scale", glm::vec3(1.0f)));
}

const glm::mat4& TransformComponent::GetWorldMatrix() const
{
	if (worldDirty)
	{
		std::stack<const TransformComponent*> chain;
		const TransformComponent* currTrans = this;
		while (currTrans != nullptr && currTrans->worldDirty)
		{
			chain.push(currTrans);
			const GameObject* p = currTrans->owner ? currTrans->owner->GetParent() : nullptr;
			currTrans = nullptr;
			while (p != nullptr)
			{
				if (const TransformComponent* pt = p->GetComponent<TransformComponent>())
				{
					currTrans = pt;
					break;
				}
				p = p->GetParent();
			}
		}

		while (!chain.empty())
		{
			const TransformComponent* t = chain.top();
			chain.pop();

			glm::mat4 parentWorld = glm::mat4(1.0f);
			const GameObject* p = t->owner ? t->owner->GetParent() : nullptr;
			while (p != nullptr)
			{
				if (const TransformComponent* pt = p->GetComponent<TransformComponent>())
				{
					parentWorld = pt->worldMatrix;
					break;
				}
				p = p->GetParent();
			}

			t->worldMatrix = parentWorld * t->GetLocalModelMatrix();
			t->worldDirty = false;
		}
	}
	return worldMatrix;
}

void TransformComponent::InvalidateWorld()
{
	// Si ya estaba sucio, por la invariante sus descendientes también lo están
	if (worldDirty) return;
	worldDirty = true;

	if (owner == nullptr) return;

	std::stack<GameObject*> stack;
	const std::vector<GameObject*>& initialChildren = owner->GetChildren();
	for (size_t i = initialChildren.size(); i > 0; --i)
	{
		stack.push(initialChildren[i - 1]);
	}

	while (!stack.empty())
	{
		GameObject* current = stack.top();
		stack.pop();
		if (!current) continue;

		if (TransformComponent* t = current->GetComponent<TransformComponent>())
		{
			if (t->IsWorldDirty()) continue;
			t->SetWorldDirty();
		}

		const std::vector<GameObject*>& children = current->GetChildren();
		for (size_t i = children.size(); i > 0; --i)
		{
			stack.push(children[i - 1]);
		}
	}
}
