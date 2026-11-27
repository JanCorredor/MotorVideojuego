#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Dentro del motor las rutas son "de proyecto": relativas a la raíz (la carpeta con Assets/) y con '/'.
// Para abrir un fichero de verdad se pasa por ToDiskPath()
namespace FileSystem
{
	// Carpetas del proyecto (rutas de proyecto)
	constexpr const char* ASSETS_DIR = "Assets";
	constexpr const char* MODELS_DIR = "Assets/Models";
	constexpr const char* TEXTURES_DIR = "Assets/Textures";
	constexpr const char* SCENES_DIR = "Assets/Scenes";
	constexpr const char* LIBRARY_DIR = "Library";
	constexpr const char* LIBRARY_MESHES_DIR = "Library/Meshes";
	constexpr const char* LIBRARY_TEXTURES_DIR = "Library/Textures";
	constexpr const char* LIBRARY_MODELS_DIR = "Library/Models";

	// Extensiones propias del motor
	constexpr const char* META_EXTENSION = ".meta";
	constexpr const char* MESH_EXTENSION = ".mesh";
	constexpr const char* TEXTURE_EXTENSION = ".tex";
	constexpr const char* MODEL_EXTENSION = ".model";
	constexpr const char* SCENE_EXTENSION = ".scene";

	// Busca la raíz del proyecto y crea las carpetas que faltan. Llamar una vez al arrancar
	bool Init();

	// Raíz tal y como se usa para abrir ficheros ("..", "." o absoluta)
	const std::string& GetProjectRoot();

	// "Assets/x.png" -> ruta que se puede abrir desde el directorio de trabajo
	std::string ToDiskPath(const std::string& projectPath);

	// Ruta de disco UTF-8 -> ruta de proyecto. "" si está fuera del proyecto
	std::string ToProjectPath(const std::string& diskPathUtf8);

	// Operaciones sobre rutas de proyecto
	bool Exists(const std::string& projectPath);
	bool IsDirectory(const std::string& projectPath);
	bool CreateDirectories(const std::string& projectPath);
	bool RemoveFile(const std::string& projectPath);
	bool RemoveFolder(const std::string& projectPath); // Borra también su contenido
	bool Rename(const std::string& fromProjectPath, const std::string& toProjectPath);

	// Copia un fichero de cualquier parte del disco (UTF-8) a una ruta de proyecto, sobrescribiendo
	bool CopyExternalFile(const std::string& diskPathUtf8, const std::string& projectPath);

	bool ReadBinary(const std::string& projectPath, std::vector<char>& outData);
	bool WriteBinary(const std::string& projectPath, const void* data, size_t size);
	bool ReadText(const std::string& projectPath, std::string& outText);
	bool WriteText(const std::string& projectPath, const std::string& text);

	// outLastWrite es opaco: solo sirve para comparar
	bool GetFileInfo(const std::string& projectPath, uint64_t& outSize, int64_t& outLastWrite);

	struct DirectoryEntry
	{
		std::string name; // "casa.fbx"
		std::string path; // "Assets/Models/casa.fbx"
		bool isDirectory = false;
	};

	// Primero carpetas y luego ficheros, en orden alfabético
	std::vector<DirectoryEntry> ListDirectory(const std::string& projectPath);
	void ListFilesRecursive(const std::string& projectPath, std::vector<std::string>& outFiles);

	// Utilidades de texto (no tocan el disco)
	std::string Normalize(const std::string& path); // '\' -> '/', sin '/' final ni "./" inicial
	std::string Join(const std::string& a, const std::string& b);
	std::string GetFileName(const std::string& path);              // "a/b/c.fbx" -> "c.fbx"
	std::string GetFileNameNoExtension(const std::string& path);   // "a/b/c.fbx" -> "c"
	std::string GetExtensionLower(const std::string& path);        // "a/b/c.FBX" -> ".fbx"
	std::string GetParentPath(const std::string& path);            // "a/b/c.fbx" -> "a/b"
	std::string ToLower(std::string text);
	bool IsInsideFolder(const std::string& path, const std::string& folder);

	// Si ya existe devuelve una alternativa libre: "x.png" -> "x (1).png"
	std::string MakeUniquePath(const std::string& projectPath);

	// Rutas guardadas en escenas y .mesh. Las rutas especiales ("Primitive:...", "(procedural)") pasan sin tocar

	// Cualquier ruta -> ruta de proyecto. Si está fuera del proyecto la devuelve normalizada
	std::string ToProjectRelativePath(const std::string& anyPath);

	// Copia a Assets/<subcarpeta> un fichero de fuera del proyecto y devuelve su ruta de proyecto.
	// Reutiliza un fichero idéntico ya copiado; con un .obj copia también su .mtl
	std::string EnsureAssetInProject(const std::string& sourcePath, const std::string& targetSubfolder);

	// Ruta guardada -> ruta de disco que existe. Si no existe, busca el fichero por nombre en Assets/ y Library/
	std::string ResolveExistingPath(const std::string& storedPath);

	// Rutas de proyecto de las escenas (.scene/.json) de Assets/Scenes
	std::vector<std::string> ListSavedSceneFiles();
}
