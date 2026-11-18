#pragma once

#include "Component.h"
#include "../../../Render/Primitives/Primitive.h"
#include "../../../ResourceManagement/Importers/MeshImporter.h"
#include "../GameObject.h"
#include <memory>
#include <filesystem>

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

	void Save(SceneFile& file) const override
	{
		file.SetBool("active", active);
		const bool hasPrim = (primitive != nullptr);
		file.SetBool("has_primitive", hasPrim);
		if (hasPrim)
		{
			const std::string& srcPath = primitive->GetSourcePath();
			const int subIdx = primitive->GetSubMeshIndex();
			std::string customPath = primitive->GetCustomMeshPath();
			if (customPath.empty())
			{
				uint64_t ownerId = owner ? (uint64_t)owner->GetUUID() : 0;
				customPath = MeshImporter::BuildCustomMeshPath(srcPath, subIdx, ownerId);
				primitive->SetCustomMeshPath(customPath);
			}

			std::error_code ec;
			if (!std::filesystem::exists(customPath, ec))
			{
				MeshImporter::SaveMeshToCustomFormat(primitive.get(), customPath);
			}

			file.SetString("source_path", srcPath);
			file.SetInt("submesh_index", subIdx);
			file.SetString("custom_mesh_path", customPath);
		}
	}

	void Load(const SceneFile& file) override
	{
		active = file.GetBool("active", true);
		const bool hasPrim = file.GetBool("has_primitive", false);
		if (hasPrim)
		{
			std::string srcPath = file.GetString("source_path", "");
			int subIdx = file.GetInt("submesh_index", -1);
			std::string customPath = file.GetString("custom_mesh_path", "");
			primitive = MeshImporter::ResolveOrLoadPrimitive(srcPath, subIdx, customPath);
		}
		else
		{
			primitive.reset();
		}
	}

private:
	std::unique_ptr<Primitive> primitive;
};
