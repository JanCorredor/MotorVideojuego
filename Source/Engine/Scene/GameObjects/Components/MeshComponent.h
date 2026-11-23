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
			std::string srcPath = primitive->GetSourcePath();
			const int subIdx = primitive->GetSubMeshIndex();

			// Garantizar que cualquier modelo externo se copie dentro de Assets/Models y se guarde con ruta relativa portable
			if (!srcPath.empty() && srcPath.rfind("Primitive:", 0) != 0)
			{
				srcPath = SceneFile::EnsureAssetInProject(srcPath, "Models");
				primitive->SetSourceInfo(srcPath, subIdx);
			}

			std::string customPath = primitive->GetCustomMeshPath();
			if (customPath.empty())
			{
				uint64_t ownerId = owner ? (uint64_t)owner->GetUUID() : 0;
				customPath = MeshImporter::BuildCustomMeshPath(srcPath, subIdx, ownerId);
			}
			else
			{
				customPath = SceneFile::ToProjectRelativePath(customPath);
			}
			primitive->SetCustomMeshPath(customPath);

			std::string customDiskPath = SceneFile::ResolveExistingPath(customPath);
			std::error_code ec;
			if (!std::filesystem::exists(customDiskPath, ec))
			{
				MeshImporter::SaveMeshToCustomFormat(primitive.get(), customPath);
			}

			const MeshImportSettings& imp = primitive->GetImportSettings();
			file.SetString("source_path", srcPath);
			file.SetInt("submesh_index", subIdx);
			file.SetString("custom_mesh_path", customPath);
			file.SetFloat("import_global_scale", imp.globalScale);
			file.SetInt("import_axis_preset", imp.axisPreset);
			file.SetBool("import_ignore_cameras", imp.ignoreCameras);
			file.SetBool("import_ignore_lights", imp.ignoreLights);
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

			MeshImportSettings imp;
			const bool hasExplicitSettings = file.HasKey("import_global_scale") || file.HasKey("import_axis_preset");
			if (hasExplicitSettings)
			{
				imp.globalScale = file.GetFloat("import_global_scale", 1.0f);
				imp.axisPreset = file.GetInt("import_axis_preset", 0);
				imp.ignoreCameras = file.GetBool("import_ignore_cameras", true);
				imp.ignoreLights = file.GetBool("import_ignore_lights", true);
			}

			primitive = MeshImporter::ResolveOrLoadPrimitive(srcPath, subIdx, customPath, imp, hasExplicitSettings);
		}
		else
		{
			primitive.reset();
		}
	}

private:
	std::unique_ptr<Primitive> primitive;
};
