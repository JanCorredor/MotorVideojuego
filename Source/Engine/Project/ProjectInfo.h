#pragma once

#include <cstdint>
#include <string>

// Un proyecto es una carpeta con Assets/, Library/ (caché, no se sube) y ProjectSettings/project.json.
// project.json va al repositorio: solo guarda datos del proyecto, nada que cambie al abrirlo o usarlo

// Versión semántica MAJOR.MINOR.PATCH del proyecto
struct SemVer
{
	int major = 0;
	int minor = 1;
	int patch = 0;

	static bool Parse(const std::string& text, SemVer& out);
	std::string ToString() const;

	void BumpMajor() { ++major; minor = 0; patch = 0; }
	void BumpMinor() { ++minor; patch = 0; }
	void BumpPatch() { ++patch; }

	bool operator==(const SemVer& o) const { return major == o.major && minor == o.minor && patch == o.patch; }
	bool operator!=(const SemVer& o) const { return !(*this == o); }
};

struct ProjectInfo
{
	static constexpr int FILE_VERSION = 1;

	std::string name;
	std::string description;
	SemVer version;
	std::string engineVersion;     // Última versión del motor que lo abrió
	std::string createdWithEngine;
	int64_t createdAt = 0;         // Segundos desde epoch

	// <rootDir>/ProjectSettings/project.json (rootDir: ruta de disco UTF-8)
	bool Load(const std::string& rootDir);
	bool Save(const std::string& rootDir) const;
};

namespace Project
{
	int64_t Now(); // Segundos desde epoch

	std::string FormatDate(int64_t timestamp);     // "2026-11-23 14:05"
	std::string FormatRelative(int64_t timestamp); // "5 minutes ago", "Yesterday"...

	// Última vez que se guardó una escena o los ajustes, según las fechas de los ficheros. 0 si no hay
	int64_t GetLastModified(const std::string& rootDir);

	// Ruta absoluta con '/' (para comparar carpetas de proyectos)
	std::string NormalizeRoot(const std::string& diskPathUtf8);

	bool FolderExists(const std::string& diskPathUtf8);

	// Tiene project.json o, al menos, una carpeta Assets/
	bool LooksLikeProject(const std::string& rootDir);

	// Carga project.json; si es un proyecto sin él, lo genera. false si la carpeta no es un proyecto
	bool LoadOrAdopt(const std::string& rootDir, ProjectInfo& out);

	// Sin \ / : * ? " < > |, no vacío, sin espacio al principio ni espacio o punto al final
	bool IsValidProjectName(const std::string& name, std::string* outError = nullptr);

	// Crea <parentDir>/<name>/ con Assets/Scenes/, ProjectSettings/project.json y un .gitignore
	bool CreateNewProject(const std::string& parentDir, const std::string& name, const std::string& description,
		std::string& outRoot, std::string* outError = nullptr);

	// Proyecto abierto en el editor
	void SetActive(const std::string& rootDir, const ProjectInfo& info);
	bool HasActive();
	const std::string& GetActiveRoot();
	ProjectInfo& GetActive();
	bool SaveActive();
}
