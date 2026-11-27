#include "MeshComponent.h"
#include "../../../../Utils/JsonHelpers.h"

void MeshComponent::Save(nlohmann::json& out) const
{
	out["active"] = active;
	if (!primitive) return;

	std::string srcPath = primitive->GetSourcePath();
	const int subIdx = primitive->GetSubMeshIndex();

	// Garantizar que cualquier modelo externo se copie dentro de Assets/Models y se guarde con ruta relativa portable
	if (!srcPath.empty() && srcPath.rfind("Primitive:", 0) != 0)
	{
		srcPath = FileSystem::EnsureAssetInProject(srcPath, "Models");
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
		customPath = FileSystem::ToProjectRelativePath(customPath);
	}
	primitive->SetCustomMeshPath(customPath);

	std::string customDiskPath = FileSystem::ResolveExistingPath(customPath);
	std::error_code ec;
	if (!std::filesystem::exists(customDiskPath, ec))
	{
		MeshImporter::SaveMeshToCustomFormat(primitive.get(), customPath);
	}

	const MeshImportSettings& imp = primitive->GetImportSettings();
	out["primitive"] = {
		{ "sourcePath", srcPath },
		{ "submeshIndex", subIdx },
		{ "customMeshPath", customPath },
		{ "import", {
			{ "globalScale", imp.globalScale },
			{ "axisPreset", imp.axisPreset },
			{ "ignoreCameras", imp.ignoreCameras },
			{ "ignoreLights", imp.ignoreLights } } }
	};
}

void MeshComponent::Load(const nlohmann::json& in)
{
	active = JsonHelpers::Get(in, "active", true);

	// Sin "primitive" el componente está vacío
	const auto prim = in.find("primitive");
	if (prim == in.end() || !prim->is_object())
	{
		primitive.reset();
		return;
	}

	std::string srcPath = JsonHelpers::Get(*prim, "sourcePath", std::string());
	int subIdx = JsonHelpers::Get(*prim, "submeshIndex", -1);
	std::string customPath = JsonHelpers::Get(*prim, "customMeshPath", std::string());

	MeshImportSettings imp;
	const auto settings = prim->find("import");
	const bool hasExplicitSettings = settings != prim->end() && settings->is_object();
	if (hasExplicitSettings)
	{
		imp.globalScale = JsonHelpers::Get(*settings, "globalScale", 1.0f);
		imp.axisPreset = JsonHelpers::Get(*settings, "axisPreset", 0);
		imp.ignoreCameras = JsonHelpers::Get(*settings, "ignoreCameras", true);
		imp.ignoreLights = JsonHelpers::Get(*settings, "ignoreLights", true);
	}

	primitive = MeshImporter::ResolveOrLoadPrimitive(srcPath, subIdx, customPath, imp, hasExplicitSettings);
}
