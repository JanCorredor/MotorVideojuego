#include "HubRegistry.h"
#include "ProjectInfo.h"
#include "../../Utils/Log.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
	std::vector<HubRegistry::Entry> entries;
	std::string defaultProjectsDir;
	bool loaded = false;

	std::string PrefDir()
	{
		char* pref = SDL_GetPrefPath("Maruja", "MarujaEngine");
		if (pref == nullptr) return std::string();
		std::string dir = pref;
		SDL_free(pref);
		return dir;
	}

	std::string Lower(std::string text)
	{
		std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return text;
	}

	// En Windows las rutas no distinguen mayúsculas
	bool SamePath(const std::string& a, const std::string& b)
	{
		return Lower(a) == Lower(b);
	}

	std::string DefaultDocumentsDir()
	{
		const char* documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
		std::string base = documents ? documents : "";
		if (base.empty())
		{
			if (const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME)) base = home;
		}
		if (base.empty()) base = ".";
		return Project::NormalizeRoot(base + "/Maruja Projects");
	}

	// Primera ejecución: registra el proyecto que acompaña al motor (el repositorio)
	void RegisterBundledProjects()
	{
		const char* basePath = SDL_GetBasePath();
		if (basePath == nullptr) return;
		const std::string exeDir = basePath;
		const std::string candidates[] = { exeDir, exeDir + "..", exeDir + "../.." };
		for (const std::string& candidate : candidates)
		{
			const std::string root = Project::NormalizeRoot(candidate);
			if (Project::FolderExists(root + "/Assets"))
			{
				HubRegistry::AddOrTouch(root, false);
				LOG("[Hub] Proyecto de ejemplo registrado: '%s'", root.c_str());
				return;
			}
		}
	}
}

namespace HubRegistry
{
	std::string GetRegistryFile()
	{
		const std::string dir = PrefDir();
		return dir.empty() ? std::string("hub.json") : dir + "hub.json";
	}

	void Load()
	{
		entries.clear();
		defaultProjectsDir = DefaultDocumentsDir();
		loaded = true;

		bool firstRun = true;
		std::ifstream file(fs::u8path(GetRegistryFile()), std::ios::binary);
		if (file)
		{
			firstRun = false;
			try
			{
				std::ostringstream stream;
				stream << file.rdbuf();
				const json root = json::parse(stream.str());
				defaultProjectsDir = root.value("defaultProjectsDir", defaultProjectsDir);
				if (root.contains("projects") && root["projects"].is_array())
				{
					for (const json& item : root["projects"])
					{
						Entry entry;
						entry.path = item.value("path", std::string());
						entry.lastOpened = item.value("lastOpened", static_cast<int64_t>(0));
						entry.pinned = item.value("pinned", false);
						entry.lastScene = item.value("lastScene", std::string());
						if (!entry.path.empty() && Find(entry.path) == nullptr) entries.push_back(entry);
					}
				}
			}
			catch (const std::exception& e)
			{
				LOG_ERROR("[Hub] hub.json inválido (%s): se empieza con una lista vacía", e.what());
			}
		}

		if (firstRun)
		{
			RegisterBundledProjects();
			Save();
		}
	}

	bool Save()
	{
		json root;
		root["defaultProjectsDir"] = defaultProjectsDir;
		json list = json::array();
		for (const Entry& entry : entries)
		{
			list.push_back({ { "path", entry.path }, { "lastOpened", entry.lastOpened }, { "pinned", entry.pinned }, { "lastScene", entry.lastScene } });
		}
		root["projects"] = list;

		std::ofstream file(fs::u8path(GetRegistryFile()), std::ios::binary | std::ios::trunc);
		if (!file)
		{
			LOG_ERROR("[Hub] No se pudo guardar '%s'", GetRegistryFile().c_str());
			return false;
		}
		const std::string text = root.dump(4);
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		return true;
	}

	std::vector<Entry>& GetEntries()
	{
		if (!loaded) Load();
		return entries;
	}

	Entry* Find(const std::string& rootDir)
	{
		const std::string normalized = Project::NormalizeRoot(rootDir);
		for (Entry& entry : entries)
		{
			if (SamePath(entry.path, normalized)) return &entry;
		}
		return nullptr;
	}

	void AddOrTouch(const std::string& rootDir, bool touch)
	{
		if (!loaded) Load();
		const std::string normalized = Project::NormalizeRoot(rootDir);
		if (normalized.empty()) return;

		Entry* entry = Find(normalized);
		if (entry == nullptr)
		{
			entries.push_back(Entry{ normalized });
			entry = &entries.back();
		}
		if (touch) entry->lastOpened = Project::Now();
	}

	void Remove(const std::string& rootDir)
	{
		const std::string normalized = Project::NormalizeRoot(rootDir);
		entries.erase(std::remove_if(entries.begin(), entries.end(),
			[&](const Entry& e) { return SamePath(e.path, normalized); }), entries.end());
	}

	std::string GetLastScene(const std::string& rootDir)
	{
		if (!loaded) Load();
		const Entry* entry = Find(rootDir);
		return entry ? entry->lastScene : std::string();
	}

	void SetLastScene(const std::string& rootDir, const std::string& scenePath)
	{
		AddOrTouch(rootDir, false);
		Entry* entry = Find(rootDir);
		if (entry == nullptr || entry->lastScene == scenePath) return;
		entry->lastScene = scenePath;
		Save(); // Se guarda ya: si el motor se cierra mal, la escena se recuerda igual
	}

	const std::string& GetDefaultProjectsDir()
	{
		if (!loaded) Load();
		return defaultProjectsDir;
	}

	void SetDefaultProjectsDir(const std::string& dir)
	{
		defaultProjectsDir = Project::NormalizeRoot(dir);
	}
}
