#pragma once

#include "../Module.h"
#include "Resource.h"
#include "ImportSettings.h"
#include "Importers/ModelImporter.h"
#include "../Render/Primitives/GeometryGenerator.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Gestiona todos los recursos: Assets/ -> importación -> Library/ (formato propio).
// - Cada asset tiene al lado un .meta (se sube al repo) con su UID, el de sus mallas y sus opciones:
//   si se borra Library se regenera con los mismos UID y las escenas siguen funcionando.
// - Al arrancar se recorre Assets: se importa lo nuevo o modificado y se limpia lo borrado.
// - RequestResource/ReleaseResource cuentan referencias; lo que se queda sin ellas se descarga
//   al principio del frame siguiente.
class ResourceManager : public Module
{
public:
	// UIDs reservados para las primitivas internas (no tienen asset ni fichero en Library)
	static constexpr UID BUILTIN_CUBE = 1;
	static constexpr UID BUILTIN_SPHERE = 2;
	static constexpr UID BUILTIN_PLANE = 3;
	static constexpr UID BUILTIN_CYLINDER = 4;
	static constexpr UID BUILTIN_PYRAMID = 5;
	static UID GetBuiltInMeshUID(PrimitiveType type);

	ResourceManager();
	~ResourceManager() override;

	bool Start() override;      // Registra las primitivas e importa todos los assets
	bool PreUpdate() override;  // Descarga los recursos que se quedaron sin referencias
	bool CleanUp() override;

	// ------------------------------- ASSETS -------------------------------------------
	static bool IsTextureFile(const std::string& path);
	static bool IsModelFile(const std::string& path);
	static bool IsSceneFile(const std::string& path);
	static bool IsImportable(const std::string& path) { return IsTextureFile(path) || IsModelFile(path); }

	// Revisa la carpeta Assets entera (importa lo nuevo / modificado, limpia lo borrado)
	void RefreshAssets();

	// Importa (o reimporta si 'force') un asset. Devuelve el UID del recurso principal (0 si falla).
	// Si el asset ya estaba importado y no ha cambiado no hace nada y devuelve su UID.
	UID ImportAsset(const std::string& assetPath, bool force = false);

	// Copia un fichero externo (ruta UTF-8, p.ej. arrastrado desde el explorador de Windows) a
	// 'destinationFolder' dentro de Assets y lo importa. Si es un modelo tambien copia sus
	// texturas. Devuelve la ruta de proyecto del nuevo asset ("" si falla).
	std::string ImportExternalFile(const std::string& diskPathUtf8, const std::string& destinationFolder);

	// Borra el asset, su .meta y sus ficheros de Library. Los GameObjects que lo usaban
	// pierden la referencia (malla vacia / textura "missing").
	bool DeleteAsset(const std::string& assetPath);
	bool DeleteFolder(const std::string& folderPath);
	// Renombra / mueve un asset (con su .meta) o una carpeta entera dentro de Assets. Los UID no
	// cambian, asi que los GameObjects que lo usan no se enteran.
	bool RenameAsset(const std::string& fromPath, const std::string& toPath);

	bool IsAssetImported(const std::string& assetPath) const;
	UID GetAssetUID(const std::string& assetPath) const;
	// UID principal + sub-recursos (las mallas de un modelo)
	std::vector<UID> GetAssetResources(const std::string& assetPath) const;

	// Opciones de importacion (se leen/escriben en el .meta). Set...() guarda y reimporta.
	bool GetTextureImportSettings(const std::string& assetPath, TextureImportSettings& outSettings) const;
	bool SetTextureImportSettings(const std::string& assetPath, const TextureImportSettings& settings);
	bool GetModelImportSettings(const std::string& assetPath, ModelImportSettings& outSettings) const;
	bool SetModelImportSettings(const std::string& assetPath, const ModelImportSettings& settings);

	// Carpeta de Assets donde se copian los ficheros arrastrados (la elige la ventana Assets)
	void SetImportFolder(const std::string& folder) { importFolder = folder; }
	const std::string& GetImportFolder() const { return importFolder; }

	// ------------------------------- RECURSOS -----------------------------------------
	// +1 referencia (lo carga si no lo estaba). nullptr si el UID no existe.
	Resource* RequestResource(UID uid);
	// -1 referencia (al llegar a 0 se descarga al principio del siguiente frame)
	void ReleaseResource(UID uid);

	// Consulta sin cambiar referencias (para el editor). nullptr si no existe.
	const Resource* GetResource(UID uid) const;

	// Todos los recursos de un tipo, ordenados por nombre (para combos y listas del editor)
	std::vector<const Resource*> GetResourcesOfType(ResourceType type) const;
	const std::unordered_map<UID, std::unique_ptr<Resource>>& GetAllResources() const { return resources; }

	size_t GetLoadedCount(ResourceType type) const;
	size_t GetTotalMemoryUsage() const;

private:
	// Lo que sabemos de cada asset importado
	struct AssetRecord
	{
		ResourceType type = ResourceType::Unknown;
		UID uid = INVALID_UID;          // recurso principal (textura o modelo)
		std::vector<UID> subResources;  // mallas de un modelo
	};

	// Contenido de un fichero .meta
	struct MetaFile
	{
		UID uid = INVALID_UID;
		ResourceType type = ResourceType::Unknown;
		TextureImportSettings textureSettings;
		ModelImportSettings modelSettings;
		std::vector<ModelImporter::MeshEntry> meshes;
	};

	// Fecha / tamano del asset y de su .meta en la ultima importacion (Library/ImportCache.json)
	struct CacheEntry
	{
		uint64_t size = 0;
		int64_t time = 0;
		int64_t metaTime = 0;
	};

	static std::string GetMetaPath(const std::string& assetPath) { return assetPath + ".meta"; }
	bool ReadMeta(const std::string& assetPath, MetaFile& outMeta) const;
	bool WriteMeta(const std::string& assetPath, const MetaFile& meta) const;

	bool LibraryFilesExist(const MetaFile& meta) const;
	bool IsCacheUpToDate(const std::string& assetPath) const;
	void UpdateCache(const std::string& assetPath);
	void LoadImportCache();
	void SaveImportCache() const;

	// Crea / actualiza los objetos Resource de un asset ya importado
	void RegisterAsset(const std::string& assetPath, const MetaFile& meta, bool reimported);
	Resource* CreateOrGetResource(UID uid, ResourceType type);
	void ReloadIfLoaded(Resource& resource);

	// Quita recursos de memoria y del mapa (antes avisa a la escena para que suelte las referencias)
	void RemoveResources(const std::vector<UID>& uids, bool deleteLibraryFiles);
	void DeleteLibraryFilesOf(const MetaFile& meta) const;
	void CleanOrphanLibraryFiles();

	// Callbacks que usa el ModelImporter para enlazar las texturas de los materiales
	UID ResolveModelTexture(const std::string& modelAssetPath, const std::string& texturePathInFile);
	UID ImportEmbeddedTexture(const std::string& modelAssetPath, const std::string& fileName, const void* data, size_t size);

	void RegisterBuiltInMeshes();

	// Índice nombre de fichero (minúsculas) -> texturas de Assets; se rehace solo cuando cambian
	void RebuildTextureIndex();

	std::unordered_map<UID, std::unique_ptr<Resource>> resources; // todos los recursos por UID
	std::unordered_map<std::string, AssetRecord> assets;          // asset (ruta) -> recursos
	std::unordered_map<std::string, CacheEntry> importCache;
	std::vector<UID> pendingUnload;                                // sin referencias: se descargan en PreUpdate
	std::unordered_set<std::string> importsInProgress;             // evita recursion infinita al importar
	std::unordered_map<std::string, std::vector<std::string>> textureIndex;
	bool textureIndexDirty = true;
	std::string importFolder = "Assets";
	bool cacheDirty = false;
	size_t importCount = 0; // importaciones reales hechas en esta sesion (para el log)
};
