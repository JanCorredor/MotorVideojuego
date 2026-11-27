#include "ResourceManager.h"
#include "ResourceMesh.h"
#include "ResourceModel.h"
#include "ResourceTexture.h"
#include "Importers/TextureImporter.h"
#include "../Engine.h"
#include "../FileSystem/FileSystem.h"
#include "../Scene/Scene.h"
#include "../../Utils/Log.h"
#include "../../Utils/PerfTimer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace
{
	constexpr const char* IMPORT_CACHE_PATH = "Library/ImportCache.json";
	constexpr int META_VERSION = 1;

	bool LessCaseInsensitive(const std::string& a, const std::string& b)
	{
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
			[](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
	}
}

UID ResourceManager::GetBuiltInMeshUID(PrimitiveType type)
{
	switch (type)
	{
	case PrimitiveType::Cube:     return BUILTIN_CUBE;
	case PrimitiveType::Sphere:   return BUILTIN_SPHERE;
	case PrimitiveType::Plane:    return BUILTIN_PLANE;
	case PrimitiveType::Cylinder: return BUILTIN_CYLINDER;
	case PrimitiveType::Pyramid:  return BUILTIN_PYRAMID;
	}
	return INVALID_UID;
}

ResourceManager::ResourceManager() : Module()
{
	name = "resources";
}

ResourceManager::~ResourceManager() = default;

// =====================================================================================
// CICLO DE VIDA DEL MODULO
// =====================================================================================
bool ResourceManager::Start()
{
	PerfTimer timer;
	RegisterBuiltInMeshes();
	LoadImportCache();
	RefreshAssets();
	LOG("[Resources] Assets revisados en %.1f ms: %zu assets (%zu importados a Library ahora), %zu recursos registrados",
		timer.ReadMs(), assets.size(), importCount, resources.size());
	return true;
}

bool ResourceManager::PreUpdate()
{
	// Recursos que se quedaron sin referencias durante el frame anterior. Se descargan aqui
	// (y no en el momento) por si alguien los vuelve a pedir enseguida, por ejemplo al
	// restaurar la escena al parar el modo Play: asi no se descargan y recargan para nada.
	if (!pendingUnload.empty())
	{
		size_t unloaded = 0;
		for (UID uid : pendingUnload)
		{
			auto it = resources.find(uid);
			if (it == resources.end()) continue;

			Resource& resource = *it->second;
			if (resource.referenceCount > 0) continue; // alguien lo volvio a pedir

			if (resource.loaded)
			{
				resource.UnloadFromMemory();
				resource.loaded = false;
				++unloaded;
			}

			// Recurso "huerfano" (su asset se borro mientras alguien lo usaba): ya se puede eliminar
			if (resource.assetPath.empty() && !resource.builtIn)
			{
				resources.erase(it);
			}
		}
		pendingUnload.clear();

		if (unloaded > 0)
		{
			LOG("[Resources] %zu recurso(s) descargado(s) de memoria (0 referencias)", unloaded);
		}
	}
	return true;
}

bool ResourceManager::CleanUp()
{
	size_t unloaded = 0;
	for (auto& entry : resources)
	{
		Resource& resource = *entry.second;
		if (resource.loaded)
		{
			resource.UnloadFromMemory();
			resource.loaded = false;
			++unloaded;
		}
	}
	resources.clear();
	assets.clear();
	pendingUnload.clear();

	if (cacheDirty)
	{
		SaveImportCache();
		cacheDirty = false;
	}

	LOG("[Resources] Limpieza: %zu recursos descargados de memoria", unloaded);
	return true;
}

void ResourceManager::RegisterBuiltInMeshes()
{
	const PrimitiveType types[] = { PrimitiveType::Cube, PrimitiveType::Sphere, PrimitiveType::Plane,
		PrimitiveType::Cylinder, PrimitiveType::Pyramid };

	for (PrimitiveType type : types)
	{
		const UID uid = GetBuiltInMeshUID(type);
		auto mesh = std::make_unique<ResourceMesh>(uid);
		mesh->SetBuiltInPrimitive(type);

		Resource& base = *mesh;
		base.name = GeometryGenerator::GetName(type);
		base.assetPath = "(Built-in)";
		resources[uid] = std::move(mesh);
	}
}

// =====================================================================================
// TIPOS DE FICHERO
// =====================================================================================
bool ResourceManager::IsTextureFile(const std::string& path)
{
	return TextureImporter::IsSupportedExtension(FileSystem::GetExtensionLower(path));
}

bool ResourceManager::IsModelFile(const std::string& path)
{
	return ModelImporter::IsSupportedExtension(FileSystem::GetExtensionLower(path));
}

bool ResourceManager::IsSceneFile(const std::string& path)
{
	return FileSystem::GetExtensionLower(path) == FileSystem::SCENE_EXTENSION;
}

// =====================================================================================
// IMPORTACION
// =====================================================================================
void ResourceManager::RefreshAssets()
{
	std::vector<std::string> files;
	FileSystem::ListFilesRecursive(FileSystem::ASSETS_DIR, files);

	// 1) Primero las texturas (los modelos las referencian) y despues los modelos
	for (const std::string& file : files)
	{
		if (IsTextureFile(file)) ImportAsset(file);
	}
	for (const std::string& file : files)
	{
		if (IsModelFile(file)) ImportAsset(file);
	}

	// 2) Ficheros .meta cuyo asset ya no existe (se borro desde fuera del motor)
	for (const std::string& file : files)
	{
		if (FileSystem::GetExtensionLower(file) != FileSystem::META_EXTENSION) continue;

		const std::string assetPath = file.substr(0, file.size() - std::char_traits<char>::length(FileSystem::META_EXTENSION));
		if (FileSystem::Exists(assetPath)) continue;

		MetaFile meta;
		if (ReadMeta(assetPath, meta)) DeleteLibraryFilesOf(meta);
		FileSystem::RemoveFile(file);
		LOG("[Resources] Eliminado '%s' (su asset ya no existe)", file.c_str());
	}

	// 3) Assets registrados que han desaparecido del disco
	std::vector<std::string> vanished;
	for (const auto& entry : assets)
	{
		if (!FileSystem::Exists(entry.first)) vanished.push_back(entry.first);
	}
	for (const std::string& path : vanished)
	{
		RemoveResources(GetAssetResources(path), true);
		assets.erase(path);
		importCache.erase(path);
		cacheDirty = true;
		LOG("[Resources] Asset '%s' eliminado: sus recursos se han quitado", path.c_str());
	}

	// 4) Ficheros de Library que no pertenecen a ningun asset
	CleanOrphanLibraryFiles();

	if (cacheDirty)
	{
		SaveImportCache();
		cacheDirty = false;
	}
}

UID ResourceManager::ImportAsset(const std::string& path, bool force)
{
	const std::string assetPath = FileSystem::Normalize(path);

	ResourceType type = ResourceType::Unknown;
	if (IsTextureFile(assetPath)) type = ResourceType::Texture;
	else if (IsModelFile(assetPath)) type = ResourceType::Model;
	if (type == ResourceType::Unknown) return INVALID_UID;

	if (!FileSystem::Exists(assetPath))
	{
		LOG_ERROR("[Resources] No existe el asset '%s'", assetPath.c_str());
		return INVALID_UID;
	}

	// Evita bucles (un modelo que pide importar una textura que pide... a si mismo)
	if (importsInProgress.count(assetPath) > 0) return GetAssetUID(assetPath);

	// 1) Leemos el .meta. Si no existe (asset nuevo) se crea con UIDs nuevos.
	MetaFile meta;
	bool hasMeta = ReadMeta(assetPath, meta) && meta.type == type;
	if (hasMeta)
	{
		// UID repetido por otro asset que SIGUE existiendo (p.ej. se copio un fichero junto con su
		// .meta): este necesita UIDs nuevos. Si el otro asset ya no existe es que el fichero se ha
		// renombrado o movido con su .meta: entonces conserva sus UIDs (las escenas siguen funcionando).
		auto taken = [this, &assetPath](UID uid)
		{
			const auto it = resources.find(uid);
			if (it == resources.end()) return false;
			const Resource& owner = *it->second;
			if (owner.builtIn) return true;
			return owner.assetPath != assetPath && !owner.assetPath.empty() && FileSystem::Exists(owner.assetPath);
		};
		bool duplicated = taken(meta.uid);
		for (const ModelImporter::MeshEntry& mesh : meta.meshes) duplicated = duplicated || taken(mesh.uid);
		if (duplicated)
		{
			LOG_WARNING("[Resources] '%s' tenia UIDs repetidos de otro asset: se generan nuevos", assetPath.c_str());
			meta.uid = GenerateUID();
			meta.meshes.clear();
			force = true;
		}
	}
	else
	{
		meta = MetaFile();
		meta.type = type;
		meta.uid = GenerateUID();
	}

	// 2) Si ya esta importado, Library esta completa y el fichero no ha cambiado: no hay nada que hacer
	if (hasMeta && !force && LibraryFilesExist(meta) && IsCacheUpToDate(assetPath))
	{
		if (assets.find(assetPath) == assets.end()) RegisterAsset(assetPath, meta, false);
		return meta.uid;
	}

	// 3) Importacion: del formato original a nuestro formato en Library
	importsInProgress.insert(assetPath);
	bool imported = false;
	if (type == ResourceType::Texture)
	{
		imported = TextureImporter::Import(assetPath, meta.textureSettings, GetLibraryPathFor(ResourceType::Texture, meta.uid));
	}
	else
	{
		imported = ModelImporter::Import(assetPath, meta.modelSettings, GetLibraryPathFor(ResourceType::Model, meta.uid), meta.meshes,
			[this, &assetPath](const std::string& texturePath) { return ResolveModelTexture(assetPath, texturePath); },
			[this, &assetPath](const std::string& fileName, const void* data, size_t size) { return ImportEmbeddedTexture(assetPath, fileName, data, size); });
	}
	importsInProgress.erase(assetPath);

	if (!imported)
	{
		LOG_ERROR("[Resources] Fallo al importar '%s'", assetPath.c_str());
		return GetAssetUID(assetPath); // si ya estaba importado de antes se conservan sus recursos
	}

	// 4) Guardamos el .meta (UIDs + opciones), la cache y registramos los recursos
	++importCount;
	WriteMeta(assetPath, meta);
	UpdateCache(assetPath);
	RegisterAsset(assetPath, meta, true);

	if (cacheDirty && importsInProgress.empty())
	{
		SaveImportCache();
		cacheDirty = false;
	}
	return meta.uid;
}

void ResourceManager::RegisterAsset(const std::string& assetPath, const MetaFile& meta, bool reimported)
{
	// Si otro registro tenia este UID, el fichero se ha renombrado o movido: ese registro ya no vale
	for (auto it = assets.begin(); it != assets.end();)
	{
		if (it->first != assetPath && it->second.uid == meta.uid)
		{
			LOG("[Resources] '%s' ahora es '%s' (mismo UID)", it->first.c_str(), assetPath.c_str());
			importCache.erase(it->first);
			cacheDirty = true;
			it = assets.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Recursos que el asset tenia antes y ya no tiene (p.ej. un modelo reimportado con menos mallas)
	const auto existing = assets.find(assetPath);
	if (existing != assets.end())
	{
		std::vector<UID> obsolete;
		if (existing->second.uid != meta.uid) obsolete.push_back(existing->second.uid);
		for (UID sub : existing->second.subResources)
		{
			const bool stillUsed = std::any_of(meta.meshes.begin(), meta.meshes.end(),
				[sub](const ModelImporter::MeshEntry& mesh) { return mesh.uid == sub; });
			if (!stillUsed) obsolete.push_back(sub);
		}
		RemoveResources(obsolete, true);
	}

	if (meta.type == ResourceType::Texture && existing == assets.end()) textureIndexDirty = true;

	AssetRecord record;
	record.type = meta.type;
	record.uid = meta.uid;

	if (Resource* mainResource = CreateOrGetResource(meta.uid, meta.type))
	{
		mainResource->name = FileSystem::GetFileName(assetPath);
		mainResource->assetPath = assetPath;
		mainResource->libraryPath = GetLibraryPathFor(meta.type, meta.uid);
		if (reimported) ReloadIfLoaded(*mainResource);
	}

	for (const ModelImporter::MeshEntry& mesh : meta.meshes)
	{
		Resource* resource = CreateOrGetResource(mesh.uid, ResourceType::Mesh);
		if (resource == nullptr) continue;
		resource->name = mesh.name;
		resource->assetPath = assetPath;
		resource->libraryPath = GetLibraryPathFor(ResourceType::Mesh, mesh.uid);
		if (reimported) ReloadIfLoaded(*resource);
		record.subResources.push_back(mesh.uid);
	}

	assets[assetPath] = std::move(record);

	// Las mallas recargadas pueden tener otra AABB: la escena debe reconstruir el octree
	if (reimported)
	{
		if (Scene* scene = Engine::GetInstance().scene.get()) scene->MarkSpacePartitioningDirty();
	}
}

Resource* ResourceManager::CreateOrGetResource(UID uid, ResourceType type)
{
	const auto it = resources.find(uid);
	if (it != resources.end())
	{
		if (it->second->type == type) return it->second.get();
		LOG_ERROR("[Resources] Conflicto de UID %llu (%s vs %s)", (unsigned long long)uid,
			ResourceTypeToString(it->second->type), ResourceTypeToString(type));
		return nullptr;
	}

	std::unique_ptr<Resource> resource;
	switch (type)
	{
	case ResourceType::Texture: resource = std::make_unique<ResourceTexture>(uid); break;
	case ResourceType::Mesh:    resource = std::make_unique<ResourceMesh>(uid); break;
	case ResourceType::Model:   resource = std::make_unique<ResourceModel>(uid); break;
	default: return nullptr;
	}

	Resource* raw = resource.get();
	resources[uid] = std::move(resource);
	return raw;
}

void ResourceManager::ReloadIfLoaded(Resource& resource)
{
	if (!resource.loaded) return;
	resource.UnloadFromMemory();
	resource.loaded = resource.LoadInMemory();
	LOG("[Resources] Recurso '%s' recargado tras reimportar (%u referencias)", resource.name.c_str(), resource.referenceCount);
}

std::string ResourceManager::ImportExternalFile(const std::string& diskPathUtf8, const std::string& destinationFolder)
{
	if (!IsImportable(diskPathUtf8) && !IsSceneFile(diskPathUtf8))
	{
		LOG_WARNING("[Resources] Formato no soportado: '%s'", diskPathUtf8.c_str());
		return std::string();
	}

	// Si el fichero ya esta dentro de Assets (se arrastro desde la propia carpeta) se importa ahi mismo
	const std::string projectPath = FileSystem::ToProjectPath(diskPathUtf8);
	if (!projectPath.empty() && FileSystem::IsInsideFolder(projectPath, FileSystem::ASSETS_DIR))
	{
		if (IsImportable(projectPath)) ImportAsset(projectPath);
		return projectPath;
	}

	// Carpeta destino: la elegida; si no hay ninguna, cada tipo a la suya (Scenes, Models, Textures)
	std::string folder = destinationFolder;
	if (IsSceneFile(diskPathUtf8)) folder = FileSystem::SCENES_DIR;
	else if (folder.empty() || folder == FileSystem::ASSETS_DIR) folder = IsModelFile(diskPathUtf8) ? FileSystem::MODELS_DIR : FileSystem::TEXTURES_DIR;
	if (!FileSystem::IsInsideFolder(folder, FileSystem::ASSETS_DIR) || !FileSystem::IsDirectory(folder))
	{
		folder = FileSystem::ASSETS_DIR;
	}

	const std::string target = FileSystem::MakeUniquePath(FileSystem::Join(folder, FileSystem::GetFileName(diskPathUtf8)));
	if (!FileSystem::CopyExternalFile(diskPathUtf8, target)) return std::string();
	LOG("[Resources] '%s' copiado a '%s'", diskPathUtf8.c_str(), target.c_str());

	// Un modelo suele necesitar sus texturas: copiamos las que esten junto al fichero original
	if (IsModelFile(target))
	{
		const std::string textureFolder = (folder == FileSystem::MODELS_DIR) ? std::string(FileSystem::TEXTURES_DIR) : folder;
		for (const std::string& texture : ModelImporter::FindExternalTextureFiles(diskPathUtf8))
		{
			const std::string textureTarget = FileSystem::Join(textureFolder, FileSystem::GetFileName(texture));
			if (!FileSystem::Exists(textureTarget) && FileSystem::CopyExternalFile(texture, textureTarget))
			{
				LOG("[Resources] Textura del modelo copiada a '%s'", textureTarget.c_str());
			}
		}
	}

	if (IsImportable(target)) ImportAsset(target);
	return target;
}

UID ResourceManager::ResolveModelTexture(const std::string& modelAssetPath, const std::string& texturePathInFile)
{
	// En el FBX la ruta puede ser absoluta del PC del artista ("E:\Work\...\a.tga"): solo sirve el nombre
	const std::string wanted = FileSystem::ToLower(FileSystem::GetFileName(texturePathInFile));
	if (wanted.empty()) return INVALID_UID;

	if (textureIndexDirty) RebuildTextureIndex();
	const auto found = textureIndex.find(wanted);
	if (found == textureIndex.end())
	{
		LOG_WARNING("[Resources] El modelo '%s' usa la textura '%s', que no esta en Assets", modelAssetPath.c_str(), texturePathInFile.c_str());
		return INVALID_UID;
	}

	// Si hay varias con el mismo nombre: junto al modelo, en sus carpetas Texture(s), en Assets/Textures y el resto
	const std::string modelDir = FileSystem::ToLower(FileSystem::GetParentPath(modelAssetPath));
	const std::string texturesDir = FileSystem::ToLower(FileSystem::TEXTURES_DIR);
	const auto priority = [&](const std::string& path)
	{
		const std::string dir = FileSystem::ToLower(FileSystem::GetParentPath(path));
		if (dir == modelDir) return 0;
		if (dir == modelDir + "/texture" || dir == modelDir + "/textures") return 1;
		if (dir == texturesDir || FileSystem::IsInsideFolder(dir, texturesDir)) return 2;
		return 3;
	};

	const std::string* best = &found->second.front();
	for (const std::string& candidate : found->second)
	{
		if (priority(candidate) < priority(*best)) best = &candidate;
	}
	return ImportAsset(*best);
}

void ResourceManager::RebuildTextureIndex()
{
	// Un solo recorrido de Assets para todas las texturas de todos los modelos
	textureIndex.clear();
	std::vector<std::string> files;
	FileSystem::ListFilesRecursive(FileSystem::ASSETS_DIR, files);
	for (const std::string& file : files)
	{
		if (IsTextureFile(file)) textureIndex[FileSystem::ToLower(FileSystem::GetFileName(file))].push_back(file);
	}
	textureIndexDirty = false;
}

UID ResourceManager::ImportEmbeddedTexture(const std::string& modelAssetPath, const std::string& fileName, const void* data, size_t size)
{
	// Las texturas que vienen dentro del modelo se extraen a Assets/Textures y se importan como cualquier otra
	(void)modelAssetPath;
	const std::string target = FileSystem::Join(FileSystem::TEXTURES_DIR, FileSystem::GetFileName(fileName));
	if (!IsTextureFile(target)) return INVALID_UID;

	if (!FileSystem::Exists(target))
	{
		if (!FileSystem::WriteBinary(target, data, size)) return INVALID_UID;
		LOG("[Resources] Textura embebida extraida: '%s'", target.c_str());
	}
	return ImportAsset(target);
}

// =====================================================================================
// BORRADO
// =====================================================================================
bool ResourceManager::DeleteAsset(const std::string& path)
{
	const std::string assetPath = FileSystem::Normalize(path);
	if (!FileSystem::IsInsideFolder(assetPath, FileSystem::ASSETS_DIR) || assetPath == FileSystem::ASSETS_DIR) return false;

	MetaFile meta;
	const bool hasMeta = ReadMeta(assetPath, meta);

	// 1) La escena suelta las referencias y se quitan los recursos (y sus ficheros de Library)
	RemoveResources(GetAssetResources(assetPath), true);
	if (hasMeta) DeleteLibraryFilesOf(meta);

	assets.erase(assetPath);
	importCache.erase(assetPath);
	SaveImportCache();
	cacheDirty = false;
	textureIndexDirty = true;

	// 2) El fichero original y su .meta
	const bool removed = FileSystem::RemoveFile(assetPath);
	if (FileSystem::Exists(GetMetaPath(assetPath))) FileSystem::RemoveFile(GetMetaPath(assetPath));

	LOG("[Resources] Asset '%s' borrado (junto con su .meta y sus ficheros de Library)", assetPath.c_str());
	return removed;
}

bool ResourceManager::DeleteFolder(const std::string& path)
{
	const std::string folder = FileSystem::Normalize(path);
	if (!FileSystem::IsInsideFolder(folder, FileSystem::ASSETS_DIR) || folder == FileSystem::ASSETS_DIR) return false;

	std::vector<std::string> files;
	FileSystem::ListFilesRecursive(folder, files);
	for (const std::string& file : files)
	{
		if (FileSystem::GetExtensionLower(file) != FileSystem::META_EXTENSION) DeleteAsset(file);
	}

	if (FileSystem::IsInsideFolder(importFolder, folder)) importFolder = FileSystem::ASSETS_DIR;
	return FileSystem::RemoveFolder(folder);
}

bool ResourceManager::RenameAsset(const std::string& fromPath, const std::string& toPath)
{
	const std::string from = FileSystem::Normalize(fromPath);
	const std::string to = FileSystem::Normalize(toPath);
	if (from == to) return true;
	if (!FileSystem::IsInsideFolder(from, FileSystem::ASSETS_DIR) || from == FileSystem::ASSETS_DIR) return false;
	if (!FileSystem::IsInsideFolder(to, FileSystem::ASSETS_DIR) || FileSystem::Exists(to)) return false;
	if (FileSystem::IsInsideFolder(to, from)) return false; // una carpeta dentro de si misma

	const bool isFolder = FileSystem::IsDirectory(from);
	if (!FileSystem::Rename(from, to))
	{
		LOG_ERROR("[Resources] Could not rename '%s' to '%s'", from.c_str(), to.c_str());
		return false;
	}
	// El .meta acompana al fichero (guarda los UID: asi las escenas siguen funcionando)
	if (!isFolder && FileSystem::Exists(GetMetaPath(from))) FileSystem::Rename(GetMetaPath(from), GetMetaPath(to));

	// Se actualizan las rutas registradas (de un fichero o de todo lo que habia dentro de la carpeta)
	const auto remap = [&](const std::string& path) -> std::string
	{
		if (path == from) return to;
		if (isFolder && FileSystem::IsInsideFolder(path, from)) return to + path.substr(from.size());
		return std::string();
	};

	std::vector<std::pair<std::string, std::string>> moved;
	for (const auto& entry : assets)
	{
		const std::string newPath = remap(entry.first);
		if (!newPath.empty()) moved.emplace_back(entry.first, newPath);
	}
	for (const auto& [oldPath, newPath] : moved)
	{
		AssetRecord record = assets[oldPath];
		assets.erase(oldPath);
		assets[newPath] = record;

		auto cacheIt = importCache.find(oldPath);
		if (cacheIt != importCache.end())
		{
			importCache[newPath] = cacheIt->second;
			importCache.erase(cacheIt);
		}

		std::vector<UID> uids = record.subResources;
		uids.push_back(record.uid);
		for (UID uid : uids)
		{
			auto it = resources.find(uid);
			if (it != resources.end()) it->second->assetPath = newPath;
		}
	}
	if (isFolder && FileSystem::IsInsideFolder(importFolder, from)) importFolder = to + importFolder.substr(from.size());

	textureIndexDirty = true;
	cacheDirty = true;
	SaveImportCache();
	cacheDirty = false;
	LOG("[Resources] Renamed '%s' -> '%s'", from.c_str(), to.c_str());
	return true;
}

void ResourceManager::RemoveResources(const std::vector<UID>& uids, bool deleteLibraryFiles)
{
	if (uids.empty()) return;

	// La escena deja de usar estos recursos (mallas vacias / textura "missing")
	if (Scene* scene = Engine::GetInstance().scene.get())
	{
		scene->OnResourcesRemoved(uids);
	}

	for (UID uid : uids)
	{
		auto it = resources.find(uid);
		if (it == resources.end()) continue;

		Resource& resource = *it->second;
		if (resource.builtIn) continue;

		if (deleteLibraryFiles && !resource.libraryPath.empty() && FileSystem::Exists(resource.libraryPath))
		{
			FileSystem::RemoveFile(resource.libraryPath);
		}

		if (resource.referenceCount > 0)
		{
			// Alguien sigue usandolo: no podemos destruir el objeto (habria punteros colgantes).
			// Se marca como huerfano y se eliminara cuando su cuenta llegue a 0.
			LOG_WARNING("[Resources] '%s' se ha borrado pero aun tiene %u referencias", resource.name.c_str(), resource.referenceCount);
			resource.assetPath.clear();
			continue;
		}

		if (resource.loaded)
		{
			resource.UnloadFromMemory();
			resource.loaded = false;
		}
		resources.erase(it);
	}
}

void ResourceManager::DeleteLibraryFilesOf(const MetaFile& meta) const
{
	const std::string mainFile = GetLibraryPathFor(meta.type, meta.uid);
	if (!mainFile.empty() && FileSystem::Exists(mainFile)) FileSystem::RemoveFile(mainFile);

	for (const ModelImporter::MeshEntry& mesh : meta.meshes)
	{
		const std::string meshFile = GetLibraryPathFor(ResourceType::Mesh, mesh.uid);
		if (FileSystem::Exists(meshFile)) FileSystem::RemoveFile(meshFile);
	}
}

void ResourceManager::CleanOrphanLibraryFiles()
{
	struct LibraryFolder { const char* path; ResourceType type; const char* extension; };
	const LibraryFolder folders[] = {
		{ FileSystem::LIBRARY_TEXTURES_DIR, ResourceType::Texture, FileSystem::TEXTURE_EXTENSION },
		{ FileSystem::LIBRARY_MESHES_DIR,   ResourceType::Mesh,    FileSystem::MESH_EXTENSION },
		{ FileSystem::LIBRARY_MODELS_DIR,   ResourceType::Model,   FileSystem::MODEL_EXTENSION }
	};

	size_t removed = 0;
	for (const LibraryFolder& folder : folders)
	{
		for (const FileSystem::DirectoryEntry& entry : FileSystem::ListDirectory(folder.path))
		{
			if (entry.isDirectory) continue;

			bool valid = false;
			if (FileSystem::GetExtensionLower(entry.name) == folder.extension)
			{
				try
				{
					const UID uid = static_cast<UID>(std::stoull(FileSystem::GetFileNameNoExtension(entry.name)));
					const auto it = resources.find(uid);
					valid = (it != resources.end() && it->second->type == folder.type && !it->second->assetPath.empty());
				}
				catch (...)
				{
					valid = false; // el nombre no es un UID: no es un fichero nuestro valido
				}
			}

			if (!valid && FileSystem::RemoveFile(entry.path)) ++removed;
		}
	}

	if (removed > 0)
	{
		LOG("[Resources] %zu fichero(s) huerfano(s) borrado(s) de Library", removed);
	}
}

// =====================================================================================
// .META E IMPORT CACHE
// =====================================================================================
bool ResourceManager::ReadMeta(const std::string& assetPath, MetaFile& outMeta) const
{
	std::string text;
	if (!FileSystem::ReadText(GetMetaPath(assetPath), text)) return false;

	const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
	if (json.is_discarded() || !json.is_object())
	{
		LOG_WARNING("[Resources] El fichero '%s' esta corrupto: se regenerara", GetMetaPath(assetPath).c_str());
		return false;
	}

	try
	{
		outMeta = MetaFile();
		outMeta.uid = json.value("uid", static_cast<UID>(INVALID_UID));
		outMeta.type = ResourceTypeFromString(json.value("type", std::string()));

		const auto settings = json.find("importSettings");
		if (settings != json.end())
		{
			if (outMeta.type == ResourceType::Texture) outMeta.textureSettings.Load(*settings);
			else if (outMeta.type == ResourceType::Model) outMeta.modelSettings.Load(*settings);
		}

		const auto meshes = json.find("meshes");
		if (meshes != json.end() && meshes->is_array())
		{
			for (const nlohmann::json& item : *meshes)
			{
				ModelImporter::MeshEntry entry;
				entry.index = item.value("index", -1);
				entry.uid = item.value("uid", static_cast<UID>(INVALID_UID));
				entry.name = item.value("name", std::string());
				if (entry.index >= 0 && entry.uid != INVALID_UID) outMeta.meshes.push_back(entry);
			}
		}
	}
	catch (const nlohmann::json::exception&)
	{
		LOG_WARNING("[Resources] El fichero '%s' tiene un formato incorrecto: se regenerara", GetMetaPath(assetPath).c_str());
		return false;
	}

	return outMeta.uid != INVALID_UID && outMeta.type != ResourceType::Unknown;
}

bool ResourceManager::WriteMeta(const std::string& assetPath, const MetaFile& meta) const
{
	nlohmann::json json;
	json["metaVersion"] = META_VERSION;
	json["uid"] = meta.uid;
	json["type"] = ResourceTypeToString(meta.type);

	nlohmann::json settings = nlohmann::json::object();
	if (meta.type == ResourceType::Texture) meta.textureSettings.Save(settings);
	else if (meta.type == ResourceType::Model) meta.modelSettings.Save(settings);
	json["importSettings"] = settings;

	if (meta.type == ResourceType::Model)
	{
		nlohmann::json meshes = nlohmann::json::array();
		for (const ModelImporter::MeshEntry& mesh : meta.meshes)
		{
			nlohmann::json entry = nlohmann::json::object();
			entry["index"] = mesh.index;
			entry["uid"] = mesh.uid;
			entry["name"] = mesh.name;
			meshes.push_back(std::move(entry));
		}
		json["meshes"] = meshes;
	}

	const std::string text = json.dump(1, '\t', false, nlohmann::json::error_handler_t::replace) + "\n";

	// Solo se escribe si cambia: los .meta van al control de versiones y no queremos tocarlos sin motivo
	std::string current;
	if (FileSystem::ReadText(GetMetaPath(assetPath), current) && current == text) return true;
	return FileSystem::WriteText(GetMetaPath(assetPath), text);
}

bool ResourceManager::LibraryFilesExist(const MetaFile& meta) const
{
	if (!FileSystem::Exists(GetLibraryPathFor(meta.type, meta.uid))) return false;
	for (const ModelImporter::MeshEntry& mesh : meta.meshes)
	{
		if (!FileSystem::Exists(GetLibraryPathFor(ResourceType::Mesh, mesh.uid))) return false;
	}
	return true;
}

bool ResourceManager::IsCacheUpToDate(const std::string& assetPath) const
{
	const auto it = importCache.find(assetPath);
	if (it == importCache.end()) return false;

	uint64_t size = 0, metaSize = 0;
	int64_t time = 0, metaTime = 0;
	if (!FileSystem::GetFileInfo(assetPath, size, time)) return false;
	if (!FileSystem::GetFileInfo(GetMetaPath(assetPath), metaSize, metaTime)) return false;

	return it->second.size == size && it->second.time == time && it->second.metaTime == metaTime;
}

void ResourceManager::UpdateCache(const std::string& assetPath)
{
	CacheEntry entry;
	uint64_t metaSize = 0;
	if (!FileSystem::GetFileInfo(assetPath, entry.size, entry.time)) return;
	if (!FileSystem::GetFileInfo(GetMetaPath(assetPath), metaSize, entry.metaTime)) return;
	importCache[assetPath] = entry;
	cacheDirty = true;
}

void ResourceManager::LoadImportCache()
{
	importCache.clear();

	std::string text;
	if (!FileSystem::ReadText(IMPORT_CACHE_PATH, text)) return;

	const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
	if (json.is_discarded() || !json.is_object()) return;

	const auto assetsJson = json.find("assets");
	if (assetsJson == json.end() || !assetsJson->is_object()) return;

	try
	{
		for (const auto& item : assetsJson->items())
		{
			CacheEntry entry;
			entry.size = item.value().value("size", static_cast<uint64_t>(0));
			entry.time = item.value().value("time", static_cast<int64_t>(0));
			entry.metaTime = item.value().value("metaTime", static_cast<int64_t>(0));
			importCache[item.key()] = entry;
		}
	}
	catch (const nlohmann::json::exception&)
	{
		importCache.clear(); // cache corrupta: se reimporta todo
	}
}

void ResourceManager::SaveImportCache() const
{
	nlohmann::json json;
	json["version"] = 1;
	nlohmann::json& assetsJson = json["assets"];
	assetsJson = nlohmann::json::object();
	for (const auto& entry : importCache)
	{
		nlohmann::json item = nlohmann::json::object();
		item["size"] = entry.second.size;
		item["time"] = entry.second.time;
		item["metaTime"] = entry.second.metaTime;
		assetsJson[entry.first] = std::move(item);
	}
	FileSystem::WriteText(IMPORT_CACHE_PATH, json.dump(1, '\t', false, nlohmann::json::error_handler_t::replace));
}

// =====================================================================================
// CONSULTAS DE ASSETS Y OPCIONES DE IMPORTACION
// =====================================================================================
bool ResourceManager::IsAssetImported(const std::string& assetPath) const
{
	return assets.find(FileSystem::Normalize(assetPath)) != assets.end();
}

UID ResourceManager::GetAssetUID(const std::string& assetPath) const
{
	const auto it = assets.find(FileSystem::Normalize(assetPath));
	return it != assets.end() ? it->second.uid : INVALID_UID;
}

std::vector<UID> ResourceManager::GetAssetResources(const std::string& assetPath) const
{
	std::vector<UID> uids;
	const auto it = assets.find(FileSystem::Normalize(assetPath));
	if (it == assets.end()) return uids;

	uids.push_back(it->second.uid);
	uids.insert(uids.end(), it->second.subResources.begin(), it->second.subResources.end());
	return uids;
}

bool ResourceManager::GetTextureImportSettings(const std::string& assetPath, TextureImportSettings& outSettings) const
{
	MetaFile meta;
	if (!ReadMeta(assetPath, meta) || meta.type != ResourceType::Texture) return false;
	outSettings = meta.textureSettings;
	return true;
}

bool ResourceManager::SetTextureImportSettings(const std::string& assetPath, const TextureImportSettings& settings)
{
	MetaFile meta;
	if (!ReadMeta(assetPath, meta) || meta.type != ResourceType::Texture) return false;
	meta.textureSettings = settings;
	if (!WriteMeta(assetPath, meta)) return false;
	return ImportAsset(assetPath, true) != INVALID_UID;
}

bool ResourceManager::GetModelImportSettings(const std::string& assetPath, ModelImportSettings& outSettings) const
{
	MetaFile meta;
	if (!ReadMeta(assetPath, meta) || meta.type != ResourceType::Model) return false;
	outSettings = meta.modelSettings;
	return true;
}

bool ResourceManager::SetModelImportSettings(const std::string& assetPath, const ModelImportSettings& settings)
{
	MetaFile meta;
	if (!ReadMeta(assetPath, meta) || meta.type != ResourceType::Model) return false;
	meta.modelSettings = settings;
	if (!WriteMeta(assetPath, meta)) return false;
	return ImportAsset(assetPath, true) != INVALID_UID;
}

// =====================================================================================
// CONTEO DE REFERENCIAS
// =====================================================================================
Resource* ResourceManager::RequestResource(UID uid)
{
	if (uid == INVALID_UID) return nullptr;

	const auto it = resources.find(uid);
	if (it == resources.end()) return nullptr;

	Resource& resource = *it->second;
	if (!resource.loaded)
	{
		resource.loaded = resource.LoadInMemory();
		if (resource.loaded)
		{
			LOG("[Resources] '%s' cargado en memoria (%s)", resource.name.c_str(), resource.GetTypeName());
		}
	}

	// Se cuenta la referencia aunque la carga fallase, para que Release() quede equilibrado
	resource.referenceCount++;
	return &resource;
}

void ResourceManager::ReleaseResource(UID uid)
{
	if (uid == INVALID_UID) return;

	const auto it = resources.find(uid);
	if (it == resources.end()) return;

	Resource& resource = *it->second;
	if (resource.referenceCount > 0) resource.referenceCount--;
	if (resource.referenceCount == 0) pendingUnload.push_back(uid);
}

const Resource* ResourceManager::GetResource(UID uid) const
{
	const auto it = resources.find(uid);
	return it != resources.end() ? it->second.get() : nullptr;
}

std::vector<const Resource*> ResourceManager::GetResourcesOfType(ResourceType type) const
{
	std::vector<const Resource*> result;
	for (const auto& entry : resources)
	{
		const Resource& resource = *entry.second;
		if (resource.type == type && !resource.assetPath.empty()) result.push_back(&resource);
	}

	// Primero las primitivas internas y despues por nombre
	std::sort(result.begin(), result.end(), [](const Resource* a, const Resource* b)
		{
			if (a->IsBuiltIn() != b->IsBuiltIn()) return a->IsBuiltIn();
			if (a->GetName() != b->GetName()) return LessCaseInsensitive(a->GetName(), b->GetName());
			return a->GetUID() < b->GetUID();
		});
	return result;
}

size_t ResourceManager::GetLoadedCount(ResourceType type) const
{
	size_t count = 0;
	for (const auto& entry : resources)
	{
		if (entry.second->type == type && entry.second->loaded) ++count;
	}
	return count;
}

size_t ResourceManager::GetTotalMemoryUsage() const
{
	size_t total = 0;
	for (const auto& entry : resources)
	{
		if (entry.second->loaded) total += entry.second->GetMemoryUsage();
	}
	return total;
}
