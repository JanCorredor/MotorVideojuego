#include "FileSystem.h"
#include "../../Utils/Log.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stack>
#include <system_error>

namespace fs = std::filesystem;

namespace
{
	std::string projectRoot = ".";
	fs::path absoluteRoot; // Se calcula una vez en Init() para no resolverla en cada conversión

	// UTF-8 con '/', igual que lo que lee fs::u8path
	std::string PathToString(const fs::path& path)
	{
		try
		{
			return path.generic_u8string();
		}
		catch (...)
		{
			return std::string();
		}
	}

	bool DirectoryExistsOnDisk(const fs::path& path)
	{
		std::error_code ec;
		return fs::is_directory(path, ec);
	}

	bool LessCaseInsensitive(const std::string& a, const std::string& b)
	{
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
			[](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
	}

	bool IsSpecialPath(const std::string& path)
	{
		return path.rfind("Primitive:", 0) == 0 || path.find("(procedural)") != std::string::npos;
	}

	bool DiskFileExists(const std::string& diskPathUtf8)
	{
		std::error_code ec;
		try
		{
			return fs::is_regular_file(fs::u8path(diskPathUtf8), ec);
		}
		catch (...)
		{
			return false;
		}
	}

	bool DiskPathExists(const std::string& diskPathUtf8)
	{
		std::error_code ec;
		try
		{
			return fs::exists(fs::u8path(diskPathUtf8), ec);
		}
		catch (...)
		{
			return false;
		}
	}

	// Escenas antiguas guardaban rutas relativas a build/ ("../Assets/...")
	std::string StripLegacyPrefixes(std::string path)
	{
		while (path.rfind("../", 0) == 0) path.erase(0, 3);
		while (path.rfind("./", 0) == 0) path.erase(0, 2);
		return path;
	}

	bool StartsWithProjectFolder(const std::string& path)
	{
		return path.rfind("Assets/", 0) == 0 || path.rfind("Library/", 0) == 0;
	}

	// Mismo tamaño y mismos bytes: evita duplicar un asset que ya se copió antes
	bool AreFilesIdentical(const std::string& diskPathA, const std::string& diskPathB)
	{
		std::error_code ec;
		const fs::path pathA = fs::u8path(diskPathA);
		const fs::path pathB = fs::u8path(diskPathB);
		if (fs::equivalent(pathA, pathB, ec)) return true;

		const uintmax_t sizeA = fs::file_size(pathA, ec);
		if (ec) return false;
		const uintmax_t sizeB = fs::file_size(pathB, ec);
		if (ec || sizeA != sizeB) return false;

		std::ifstream fa(pathA, std::ios::binary);
		std::ifstream fb(pathB, std::ios::binary);
		if (!fa.is_open() || !fb.is_open()) return false;

		char bufA[8192];
		char bufB[8192];
		while (fa && fb)
		{
			fa.read(bufA, sizeof(bufA));
			fb.read(bufB, sizeof(bufB));
			const std::streamsize countA = fa.gcount();
			const std::streamsize countB = fb.gcount();
			if (countA != countB) return false;
			if (countA > 0 && std::memcmp(bufA, bufB, static_cast<size_t>(countA)) != 0) return false;
		}
		return true;
	}

	// Búsqueda iterativa por nombre sin distinguir mayúsculas. Devuelve ruta de disco o ""
	std::string FindFileByNameCaseInsensitive(const std::string& rootDiskDir, const std::string& targetFilename)
	{
		if (rootDiskDir.empty() || targetFilename.empty()) return "";
		std::error_code ec;
		const fs::path root = fs::u8path(rootDiskDir);
		if (!fs::is_directory(root, ec)) return "";

		const std::string targetLower = FileSystem::ToLower(targetFilename);
		std::stack<fs::path> dirStack;
		dirStack.push(root);

		while (!dirStack.empty())
		{
			const fs::path currentDir = dirStack.top();
			dirStack.pop();

			for (fs::directory_iterator it(currentDir, ec), end; !ec && it != end; it.increment(ec))
			{
				std::error_code entryEc;
				if (it->is_directory(entryEc))
				{
					dirStack.push(it->path());
				}
				else if (it->is_regular_file(entryEc) && FileSystem::ToLower(PathToString(it->path().filename())) == targetLower)
				{
					return FileSystem::Normalize(PathToString(it->path()));
				}
			}
		}
		return "";
	}
}

namespace FileSystem
{
	bool Init()
	{
		// Orden: directorio de trabajo, su padre (VS ejecuta desde build/) y la carpeta del exe con sus padres
		std::vector<fs::path> candidates = { fs::path("."), fs::path("..") };

		if (const char* basePath = SDL_GetBasePath())
		{
			try
			{
				const fs::path exeDir = fs::u8path(basePath); // SDL la da en UTF-8
				candidates.push_back(exeDir);
				candidates.push_back(exeDir / "..");
				candidates.push_back(exeDir / ".." / "..");
			}
			catch (...)
			{
				LOG_WARNING("[FileSystem] No se pudo interpretar la carpeta del ejecutable");
			}
		}

		bool found = false;
		for (const fs::path& candidate : candidates)
		{
			if (DirectoryExistsOnDisk(candidate / ASSETS_DIR))
			{
				std::string root = PathToString(candidate.lexically_normal());
				if (root.empty()) continue;
				if (root.size() > 1 && root.back() == '/') root.pop_back();
				projectRoot = root;
				found = true;
				break;
			}
		}

		if (!found)
		{
			projectRoot = ".";
			LOG_WARNING("[FileSystem] No se encontró la carpeta Assets. Se crea una vacía en el directorio de trabajo.");
		}

		std::error_code ec;
		absoluteRoot = fs::weakly_canonical(fs::u8path(projectRoot), ec);
		if (ec) absoluteRoot.clear();

		bool ok = CreateDirectories(ASSETS_DIR);
		ok &= CreateDirectories(MODELS_DIR);
		ok &= CreateDirectories(TEXTURES_DIR);
		ok &= CreateDirectories(SCENES_DIR);
		ok &= CreateDirectories(LIBRARY_MESHES_DIR);

		LOG("[FileSystem] Raíz del proyecto: '%s'", projectRoot.c_str());
		return ok;
	}

	const std::string& GetProjectRoot()
	{
		return projectRoot;
	}

	std::string ToDiskPath(const std::string& projectPath)
	{
		if (projectRoot.empty() || projectRoot == ".") return projectPath;
		if (projectPath.empty()) return projectRoot;
		return projectRoot + "/" + projectPath;
	}

	std::string ToProjectPath(const std::string& diskPathUtf8)
	{
		std::error_code ec;
		fs::path inputPath;
		try
		{
			inputPath = fs::u8path(diskPathUtf8);
		}
		catch (...)
		{
			return std::string();
		}
		const fs::path absoluteFile = fs::weakly_canonical(inputPath, ec);
		if (ec) return std::string();
		if (absoluteRoot.empty()) return std::string(); // Init() aún no se ha llamado

		const std::string result = PathToString(absoluteFile.lexically_relative(absoluteRoot));
		// Si hay que salir de la raíz para llegar al fichero, no es del proyecto
		if (result.empty() || result == "." || result.rfind("..", 0) == 0) return std::string();
		return Normalize(result);
	}

	bool Exists(const std::string& projectPath)
	{
		std::error_code ec;
		return fs::exists(fs::u8path(ToDiskPath(projectPath)), ec);
	}

	bool IsDirectory(const std::string& projectPath)
	{
		return DirectoryExistsOnDisk(fs::u8path(ToDiskPath(projectPath)));
	}

	bool CreateDirectories(const std::string& projectPath)
	{
		std::error_code ec;
		const fs::path path = fs::u8path(ToDiskPath(projectPath));
		if (fs::is_directory(path, ec)) return true;
		fs::create_directories(path, ec);
		if (ec)
		{
			LOG_ERROR("[FileSystem] No se pudo crear la carpeta '%s': %s", projectPath.c_str(), ec.message().c_str());
			return false;
		}
		return true;
	}

	bool RemoveFile(const std::string& projectPath)
	{
		std::error_code ec;
		const bool removed = fs::remove(fs::u8path(ToDiskPath(projectPath)), ec);
		if (ec)
		{
			LOG_ERROR("[FileSystem] No se pudo borrar '%s': %s", projectPath.c_str(), ec.message().c_str());
			return false;
		}
		return removed;
	}

	bool RemoveFolder(const std::string& projectPath)
	{
		std::error_code ec;
		fs::remove_all(fs::u8path(ToDiskPath(projectPath)), ec);
		if (ec)
		{
			LOG_ERROR("[FileSystem] No se pudo borrar la carpeta '%s': %s", projectPath.c_str(), ec.message().c_str());
			return false;
		}
		return true;
	}

	bool Rename(const std::string& fromProjectPath, const std::string& toProjectPath)
	{
		std::error_code ec;
		fs::rename(fs::u8path(ToDiskPath(fromProjectPath)), fs::u8path(ToDiskPath(toProjectPath)), ec);
		if (ec)
		{
			LOG_ERROR("[FileSystem] No se pudo renombrar '%s' a '%s': %s",
				fromProjectPath.c_str(), toProjectPath.c_str(), ec.message().c_str());
			return false;
		}
		return true;
	}

	bool CopyExternalFile(const std::string& diskPathUtf8, const std::string& projectPath)
	{
		CreateDirectories(GetParentPath(projectPath));

		std::error_code ec;
		try
		{
			fs::copy_file(fs::u8path(diskPathUtf8), fs::u8path(ToDiskPath(projectPath)), fs::copy_options::overwrite_existing, ec);
		}
		catch (...)
		{
			ec = std::make_error_code(std::errc::invalid_argument);
		}
		if (ec)
		{
			LOG_ERROR("[FileSystem] No se pudo copiar '%s' a '%s': %s", diskPathUtf8.c_str(), projectPath.c_str(), ec.message().c_str());
			return false;
		}
		return true;
	}

	bool ReadBinary(const std::string& projectPath, std::vector<char>& outData)
	{
		outData.clear();
		std::ifstream file(fs::u8path(ToDiskPath(projectPath)), std::ios::binary | std::ios::ate);
		if (!file.is_open()) return false;

		const std::streamsize size = file.tellg();
		if (size < 0) return false;
		file.seekg(0, std::ios::beg);

		outData.resize(static_cast<size_t>(size));
		if (size > 0 && !file.read(outData.data(), size))
		{
			outData.clear();
			return false;
		}
		return true;
	}

	bool WriteBinary(const std::string& projectPath, const void* data, size_t size)
	{
		CreateDirectories(GetParentPath(projectPath));

		std::ofstream file(fs::u8path(ToDiskPath(projectPath)), std::ios::binary | std::ios::trunc);
		if (!file.is_open())
		{
			LOG_ERROR("[FileSystem] No se pudo abrir '%s' para escribir", projectPath.c_str());
			return false;
		}
		if (size > 0)
		{
			file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
		}
		return file.good();
	}

	bool ReadText(const std::string& projectPath, std::string& outText)
	{
		std::vector<char> data;
		if (!ReadBinary(projectPath, data)) return false;
		outText.assign(data.begin(), data.end());
		return true;
	}

	bool WriteText(const std::string& projectPath, const std::string& text)
	{
		return WriteBinary(projectPath, text.data(), text.size());
	}

	bool GetFileInfo(const std::string& projectPath, uint64_t& outSize, int64_t& outLastWrite)
	{
		std::error_code ec;
		const fs::path path = fs::u8path(ToDiskPath(projectPath));

		const auto size = fs::file_size(path, ec);
		if (ec) return false;
		const auto time = fs::last_write_time(path, ec);
		if (ec) return false;

		outSize = static_cast<uint64_t>(size);
		outLastWrite = static_cast<int64_t>(time.time_since_epoch().count());
		return true;
	}

	std::vector<DirectoryEntry> ListDirectory(const std::string& projectPath)
	{
		std::vector<DirectoryEntry> entries;

		std::error_code ec;
		fs::directory_iterator it(fs::u8path(ToDiskPath(projectPath)), ec);
		if (ec) return entries;

		for (const fs::directory_entry& item : it)
		{
			const std::string name = PathToString(item.path().filename());
			if (name.empty()) continue; // Nombre no representable

			DirectoryEntry entry;
			entry.name = name;
			entry.path = Join(projectPath, name);
			std::error_code typeError;
			entry.isDirectory = item.is_directory(typeError);
			entries.push_back(std::move(entry));
		}

		std::sort(entries.begin(), entries.end(), [](const DirectoryEntry& a, const DirectoryEntry& b)
			{
				if (a.isDirectory != b.isDirectory) return a.isDirectory;
				return LessCaseInsensitive(a.name, b.name);
			});
		return entries;
	}

	void ListFilesRecursive(const std::string& projectPath, std::vector<std::string>& outFiles)
	{
		const fs::path start = fs::u8path(ToDiskPath(projectPath));
		std::error_code ec;
		fs::recursive_directory_iterator it(start, fs::directory_options::skip_permission_denied, ec);
		if (ec) return;

		const fs::recursive_directory_iterator end;
		while (it != end)
		{
			std::error_code typeError;
			if (it->is_regular_file(typeError))
			{
				const std::string relative = PathToString(it->path().lexically_relative(start));
				if (!relative.empty())
				{
					outFiles.push_back(Join(projectPath, relative));
				}
			}
			it.increment(ec);
			if (ec) break;
		}
		std::sort(outFiles.begin(), outFiles.end());
	}

	std::string Normalize(const std::string& path)
	{
		std::string result;
		result.reserve(path.size());
		for (char c : path)
		{
			const char current = (c == '\\') ? '/' : c;
			if (current == '/' && !result.empty() && result.back() == '/') continue; // Sin "//"
			result.push_back(current);
		}
		while (result.rfind("./", 0) == 0) result.erase(0, 2);
		while (result.size() > 1 && result.back() == '/') result.pop_back();
		return result;
	}

	std::string Join(const std::string& a, const std::string& b)
	{
		if (a.empty()) return Normalize(b);
		if (b.empty()) return Normalize(a);
		return Normalize(a + "/" + b);
	}

	std::string GetFileName(const std::string& path)
	{
		const std::string normalized = Normalize(path);
		const size_t slash = normalized.find_last_of('/');
		return (slash == std::string::npos) ? normalized : normalized.substr(slash + 1);
	}

	std::string GetFileNameNoExtension(const std::string& path)
	{
		const std::string name = GetFileName(path);
		const size_t dot = name.find_last_of('.');
		return (dot == std::string::npos || dot == 0) ? name : name.substr(0, dot);
	}

	std::string GetExtensionLower(const std::string& path)
	{
		const std::string name = GetFileName(path);
		const size_t dot = name.find_last_of('.');
		if (dot == std::string::npos || dot == 0) return std::string();
		return ToLower(name.substr(dot));
	}

	std::string GetParentPath(const std::string& path)
	{
		const std::string normalized = Normalize(path);
		const size_t slash = normalized.find_last_of('/');
		return (slash == std::string::npos) ? std::string() : normalized.substr(0, slash);
	}

	std::string ToLower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	bool IsInsideFolder(const std::string& path, const std::string& folder)
	{
		const std::string p = Normalize(path);
		const std::string f = Normalize(folder);
		if (p == f) return true;
		return p.size() > f.size() && p.compare(0, f.size(), f) == 0 && p[f.size()] == '/';
	}

	std::string MakeUniquePath(const std::string& projectPath)
	{
		if (!Exists(projectPath)) return projectPath;

		const std::string folder = GetParentPath(projectPath);
		const std::string stem = GetFileNameNoExtension(projectPath);
		const std::string name = GetFileName(projectPath);
		const size_t dot = name.find_last_of('.');
		const std::string extension = (dot == std::string::npos || dot == 0) ? std::string() : name.substr(dot);

		for (int i = 1; i < 10000; ++i)
		{
			const std::string candidate = Join(folder, stem + " (" + std::to_string(i) + ")" + extension);
			if (!Exists(candidate)) return candidate;
		}
		return projectPath;
	}

	std::string ToProjectRelativePath(const std::string& anyPath)
	{
		if (anyPath.empty()) return "";
		if (IsSpecialPath(anyPath)) return anyPath;

		// Ya es ruta de proyecto: no hace falta tocar el disco
		const std::string norm = Normalize(anyPath);
		if (StartsWithProjectFolder(norm)) return norm;

		const std::string projectPath = ToProjectPath(norm);
		if (!projectPath.empty()) return projectPath;

		const std::string stripped = StripLegacyPrefixes(norm);
		if (StartsWithProjectFolder(stripped)) return stripped;

		return norm;
	}

	std::string EnsureAssetInProject(const std::string& sourcePath, const std::string& targetSubfolder)
	{
		if (sourcePath.empty()) return "";
		if (IsSpecialPath(sourcePath)) return sourcePath;

		const std::string resolvedSource = ResolveExistingPath(sourcePath);
		if (!DiskFileExists(resolvedSource))
		{
			// Puede resolverse más tarde: se guarda la ruta normalizada
			return ToProjectRelativePath(sourcePath);
		}

		// Ya está dentro de Assets/: no se duplica
		const std::string sourceProjectPath = ToProjectPath(resolvedSource);
		if (!sourceProjectPath.empty() && IsInsideFolder(sourceProjectPath, ASSETS_DIR))
		{
			return sourceProjectPath;
		}

		const std::string destFolder = targetSubfolder.empty() ? std::string(ASSETS_DIR) : Join(ASSETS_DIR, targetSubfolder);
		CreateDirectories(destFolder);

		const std::string fileName = GetFileName(resolvedSource);
		if (fileName.empty()) return ToProjectRelativePath(resolvedSource);
		const std::string stem = GetFileNameNoExtension(fileName);
		const std::string extension = fileName.substr(stem.size());

		std::string destPath = Join(destFolder, fileName);
		for (int suffix = 1; Exists(destPath); ++suffix)
		{
			if (AreFilesIdentical(resolvedSource, ToDiskPath(destPath)))
			{
				LOG("[Assets] Archivo externo ya existía en el proyecto (reutilizado): '%s'", destPath.c_str());
				return destPath;
			}
			// Mismo nombre pero otro contenido: no se sobrescribe
			destPath = Join(destFolder, stem + "_" + std::to_string(suffix) + extension);
		}

		if (!CopyExternalFile(resolvedSource, destPath))
		{
			return ToProjectRelativePath(resolvedSource);
		}

		if (GetExtensionLower(fileName) == ".obj")
		{
			const std::string mtlSource = Join(GetParentPath(resolvedSource), stem + ".mtl");
			if (DiskFileExists(mtlSource))
			{
				CopyExternalFile(mtlSource, Join(GetParentPath(destPath), GetFileNameNoExtension(destPath) + ".mtl"));
			}
		}

		LOG("[Assets] Archivo externo copiado al proyecto: '%s' -> '%s'", resolvedSource.c_str(), destPath.c_str());
		return destPath;
	}

	std::string ResolveExistingPath(const std::string& storedPath)
	{
		if (storedPath.empty()) return storedPath;
		if (IsSpecialPath(storedPath)) return storedPath;

		const std::string norm = Normalize(storedPath);
		bool isAbsolute = false;
		try
		{
			isAbsolute = fs::u8path(norm).is_absolute();
		}
		catch (...) {}

		if (!isAbsolute)
		{
			// 1. Relativa a la raíz ("Assets/...", "Library/...")
			const std::string rootCandidate = ToDiskPath(norm);
			if (DiskPathExists(rootCandidate)) return rootCandidate;

			// 2. Formato antiguo con "../"
			const std::string stripped = StripLegacyPrefixes(norm);
			if (stripped != norm)
			{
				const std::string strippedCandidate = ToDiskPath(stripped);
				if (DiskPathExists(strippedCandidate)) return strippedCandidate;
			}
		}

		// 3. Tal cual: absoluta o relativa al directorio de trabajo
		if (DiskPathExists(norm)) return norm;
		if (DiskPathExists(storedPath)) return storedPath;

		// 4. Por nombre en la carpeta que toca según la extensión y, si no, en todo Assets/
		const std::string fileName = GetFileName(norm);
		if (fileName.empty()) return storedPath;

		const std::string extension = GetExtensionLower(fileName);
		const char* folder = nullptr;
		if (extension == MESH_EXTENSION) folder = LIBRARY_MESHES_DIR;
		else if (extension == SCENE_EXTENSION || extension == ".json") folder = SCENES_DIR;
		else if (extension == ".fbx" || extension == ".obj" || extension == ".dae") folder = MODELS_DIR;
		else if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
			extension == ".dds" || extension == ".tga" || extension == ".bmp") folder = TEXTURES_DIR;

		if (folder)
		{
			const std::string found = FindFileByNameCaseInsensitive(ToDiskPath(folder), fileName);
			if (!found.empty()) return found;
		}

		const std::string foundInAssets = FindFileByNameCaseInsensitive(ToDiskPath(ASSETS_DIR), fileName);
		if (!foundInAssets.empty()) return foundInAssets;

		return storedPath;
	}

	std::vector<std::string> ListSavedSceneFiles()
	{
		std::vector<std::string> results;
		for (const DirectoryEntry& entry : ListDirectory(SCENES_DIR))
		{
			if (entry.isDirectory) continue;
			const std::string extension = GetExtensionLower(entry.name);
			if (extension == SCENE_EXTENSION || extension == ".json")
			{
				results.push_back(entry.path);
			}
		}
		return results;
	}
}
