#include "ProjectInfo.h"
#include "../../Utils/Log.h"
#include "Version.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <charconv>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
	std::string activeRoot;
	ProjectInfo activeInfo;
	bool hasActive = false;

	fs::path U8(const std::string& path)
	{
		try { return fs::u8path(path); }
		catch (...) { return fs::path(); }
	}

	std::string ToU8(const fs::path& path)
	{
		try
		{
			const auto text = path.generic_u8string();
			return std::string(text.begin(), text.end());
		}
		catch (...) { return std::string(); }
	}

	std::string ProjectFilePath(const std::string& rootDir)
	{
		return rootDir + "/ProjectSettings/project.json";
	}

	bool WriteFileUtf8(const std::string& path, const std::string& text)
	{
		std::error_code ec;
		fs::create_directories(U8(path).parent_path(), ec);
		std::ofstream file(U8(path), std::ios::binary | std::ios::trunc);
		if (!file) return false;
		file.write(text.data(), static_cast<std::streamsize>(text.size()));
		return static_cast<bool>(file);
	}

	bool ReadFileUtf8(const std::string& path, std::string& out)
	{
		std::ifstream file(U8(path), std::ios::binary);
		if (!file) return false;
		std::ostringstream stream;
		stream << file.rdbuf();
		out = stream.str();
		return true;
	}

	// C++17 no convierte el reloj de ficheros al del sistema: se traslada por la diferencia entre ambos ahora
	int64_t ToEpochSeconds(fs::file_time_type fileTime)
	{
		using namespace std::chrono;
		const auto systemTime = time_point_cast<system_clock::duration>(
			fileTime - fs::file_time_type::clock::now() + system_clock::now());
		return duration_cast<seconds>(systemTime.time_since_epoch()).count();
	}
}

bool SemVer::Parse(const std::string& text, SemVer& out)
{
	// Lee hasta tres enteros separados por '.'; acepta sufijos como "1.2.3-beta"
	int parts[3] = { 0, 0, 0 };
	int count = 0;
	const char* p = text.data();
	const char* end = p + text.size();
	while (p != end && *p == ' ') ++p;

	while (count < 3)
	{
		int value = 0;
		const auto [next, error] = std::from_chars(p, end, value);
		if (error != std::errc() || value < 0) break;
		parts[count++] = value;
		p = next;
		if (p == end || *p != '.') break;
		++p;
	}

	if (count < 1) return false;
	out.major = parts[0];
	out.minor = parts[1];
	out.patch = parts[2];
	return true;
}

std::string SemVer::ToString() const
{
	return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

bool ProjectInfo::Load(const std::string& rootDir)
{
	std::string text;
	if (!ReadFileUtf8(ProjectFilePath(rootDir), text)) return false;

	try
	{
		const json root = json::parse(text);
		name = root.value("name", std::string());
		description = root.value("description", std::string());
		if (!SemVer::Parse(root.value("version", std::string("0.1.0")), version)) version = SemVer();
		engineVersion = root.value("engineVersion", std::string());
		createdWithEngine = root.value("createdWithEngine", engineVersion);
		createdAt = root.value("createdAt", static_cast<int64_t>(0));
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("[Project] project.json inválido en '%s': %s", rootDir.c_str(), e.what());
		return false;
	}

	if (name.empty()) name = ToU8(U8(rootDir).filename());
	return true;
}

bool ProjectInfo::Save(const std::string& rootDir) const
{
	json root;
	root["fileVersion"] = FILE_VERSION;
	root["name"] = name;
	root["description"] = description;
	root["version"] = version.ToString();
	root["engine"] = ENGINE_NAME;
	root["engineVersion"] = engineVersion;
	root["createdWithEngine"] = createdWithEngine;
	root["createdAt"] = createdAt;

	if (!WriteFileUtf8(ProjectFilePath(rootDir), root.dump(4)))
	{
		LOG_ERROR("[Project] No se pudo guardar '%s'", ProjectFilePath(rootDir).c_str());
		return false;
	}
	return true;
}

namespace Project
{
	int64_t Now()
	{
		return static_cast<int64_t>(std::time(nullptr));
	}

	std::string FormatDate(int64_t timestamp)
	{
		if (timestamp <= 0) return "-";
		const std::time_t t = static_cast<std::time_t>(timestamp);
		std::tm local{};
#if defined(_WIN32)
		localtime_s(&local, &t);
#else
		localtime_r(&t, &local);
#endif
		char buffer[32];
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);
		return buffer;
	}

	std::string FormatRelative(int64_t timestamp)
	{
		if (timestamp <= 0) return "Never";
		const int64_t diff = Now() - timestamp;
		if (diff < 60) return "Just now";
		if (diff < 3600) { const int64_t m = diff / 60; return std::to_string(m) + (m == 1 ? " minute ago" : " minutes ago"); }
		if (diff < 86400) { const int64_t h = diff / 3600; return std::to_string(h) + (h == 1 ? " hour ago" : " hours ago"); }
		const int64_t d = diff / 86400;
		if (d == 1) return "Yesterday";
		if (d < 30) return std::to_string(d) + " days ago";
		if (d < 365) { const int64_t mo = d / 30; return std::to_string(mo) + (mo == 1 ? " month ago" : " months ago"); }
		return FormatDate(timestamp);
	}

	int64_t GetLastModified(const std::string& rootDir)
	{
		int64_t newest = 0;
		std::error_code ec;

		const fs::path projectFile = U8(ProjectFilePath(rootDir));
		const auto projectTime = fs::last_write_time(projectFile, ec);
		if (!ec) newest = ToEpochSeconds(projectTime);

		fs::recursive_directory_iterator it(U8(rootDir + "/Assets/Scenes"), fs::directory_options::skip_permission_denied, ec);
		const fs::recursive_directory_iterator end;
		for (; !ec && it != end; it.increment(ec))
		{
			std::error_code entryEc;
			if (!it->is_regular_file(entryEc)) continue;
			const auto sceneTime = it->last_write_time(entryEc);
			if (!entryEc) newest = std::max(newest, ToEpochSeconds(sceneTime));
		}
		return newest;
	}

	std::string NormalizeRoot(const std::string& diskPathUtf8)
	{
		if (diskPathUtf8.empty()) return std::string();
		std::error_code ec;
		fs::path path = fs::absolute(U8(diskPathUtf8), ec);
		if (ec) path = U8(diskPathUtf8);
		std::string result = ToU8(path.lexically_normal());
		while (result.size() > 3 && result.back() == '/') result.pop_back();
		return result;
	}

	bool FolderExists(const std::string& diskPathUtf8)
	{
		std::error_code ec;
		return fs::is_directory(U8(diskPathUtf8), ec);
	}

	bool LooksLikeProject(const std::string& rootDir)
	{
		std::error_code ec;
		return fs::is_regular_file(U8(ProjectFilePath(rootDir)), ec) || fs::is_directory(U8(rootDir + "/Assets"), ec);
	}

	bool LoadOrAdopt(const std::string& rootDir, ProjectInfo& out)
	{
		if (out.Load(rootDir)) return true;
		if (!LooksLikeProject(rootDir)) return false;

		// Proyecto creado antes de existir project.json
		out = ProjectInfo();
		out.name = ToU8(U8(rootDir).filename());
		out.engineVersion = ENGINE_VERSION_STRING;
		out.createdWithEngine = ENGINE_VERSION_STRING;
		out.createdAt = Now();
		LOG("[Project] '%s' no tenía project.json: se genera uno nuevo", rootDir.c_str());
		return out.Save(rootDir);
	}

	bool IsValidProjectName(const std::string& name, std::string* outError)
	{
		auto fail = [&](const char* message) { if (outError) *outError = message; return false; };
		if (name.empty()) return fail("Enter a project name");
		if (name.find_first_of("\\/:*?\"<>|") != std::string::npos) return fail("The name cannot contain \\ / : * ? \" < > |");
		if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') return fail("The name cannot start or end with a space or end with a dot");
		return true;
	}

	bool CreateNewProject(const std::string& parentDir, const std::string& name, const std::string& description,
		std::string& outRoot, std::string* outError)
	{
		auto fail = [&](const std::string& message) { if (outError) *outError = message; LOG_ERROR("[Project] %s", message.c_str()); return false; };

		if (!IsValidProjectName(name, outError)) return false;
		if (parentDir.empty() || !FolderExists(parentDir)) return fail("The location does not exist");

		const std::string root = NormalizeRoot(parentDir + "/" + name);
		std::error_code ec;
		if (fs::exists(U8(root), ec)) return fail("A folder with that name already exists in this location");

		fs::create_directories(U8(root + "/Assets/Scenes"), ec);
		if (ec) return fail("Could not create the project folder: " + ec.message());
		fs::create_directories(U8(root + "/ProjectSettings"), ec);

		ProjectInfo info;
		info.name = name;
		info.description = description;
		info.version = SemVer{ 0, 1, 0 };
		info.engineVersion = ENGINE_VERSION_STRING;
		info.createdWithEngine = ENGINE_VERSION_STRING;
		info.createdAt = Now();
		if (!info.Save(root)) return fail("Could not write project.json");

		WriteFileUtf8(root + "/.gitignore",
			"# Generado por " ENGINE_NAME "\n"
			"# Library/ es la caché de importación: se regenera a partir de Assets/ y los .meta\n"
			"Library/\n"
			"ProjectSettings/EditorLayout.ini\n"
			"# Builds y releases del juego\n"
			"Builds/\n");

		outRoot = root;
		LOG("[Project] Proyecto '%s' creado en '%s'", name.c_str(), root.c_str());
		return true;
	}

	void SetActive(const std::string& rootDir, const ProjectInfo& info)
	{
		activeRoot = rootDir;
		activeInfo = info;
		hasActive = true;
	}

	bool HasActive() { return hasActive; }
	const std::string& GetActiveRoot() { return activeRoot; }
	ProjectInfo& GetActive() { return activeInfo; }

	bool SaveActive()
	{
		return hasActive && activeInfo.Save(activeRoot);
	}
}
