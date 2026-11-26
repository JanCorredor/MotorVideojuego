#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Proyectos que conoce el usuario y sus preferencias. Es por usuario, fuera de los proyectos:
// %APPDATA%\Maruja\MarujaEngine\hub.json. Aquí va lo que cambia al usar un proyecto (no en project.json)
namespace HubRegistry
{
	struct Entry
	{
		std::string path;        // Raíz del proyecto (absoluta, con '/')
		int64_t lastOpened = 0;  // Segundos desde epoch; 0 = nunca abierto desde el Hub
		bool pinned = false;
		std::string lastScene;   // Ruta de proyecto de la última escena abierta o guardada
	};

	// La primera vez registra el proyecto de ejemplo que acompaña al motor
	void Load();
	bool Save();

	std::vector<Entry>& GetEntries();

	// Lo añade si no estaba y, con touch, actualiza su fecha de última apertura
	void AddOrTouch(const std::string& rootDir, bool touch = true);
	void Remove(const std::string& rootDir);
	Entry* Find(const std::string& rootDir);

	// "" si el proyecto no está registrado o no tiene escena guardada
	std::string GetLastScene(const std::string& rootDir);
	void SetLastScene(const std::string& rootDir, const std::string& scenePath);

	// Dónde se crean los proyectos nuevos (Documentos/Maruja Projects)
	const std::string& GetDefaultProjectsDir();
	void SetDefaultProjectsDir(const std::string& dir);

	std::string GetRegistryFile();
}
