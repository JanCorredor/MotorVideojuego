#include "ResourceModel.h"
#include "Importers/ModelImporter.h"
#include "../../Utils/Log.h"

bool ResourceModel::LoadInMemory()
{
	if (!ModelImporter::LoadModelFile(libraryPath, nodes))
	{
		LOG_ERROR("[Resources] No se pudo cargar el modelo '%s' (%s)", name.c_str(), libraryPath.c_str());
		nodes.clear();
		return false;
	}
	return true;
}

void ResourceModel::UnloadFromMemory()
{
	nodes.clear();
	nodes.shrink_to_fit();
}
