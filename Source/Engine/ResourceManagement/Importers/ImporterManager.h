#pragma once

#include "../Resource.h"
#include <string>

class GameObject;

// Fichero arrastrado a la ventana: se copia a Assets y se importa; un modelo se instancia, una
// textura se asigna al seleccionado (y a sus hijos) y una escena se abre
class ImporterManager
{
public:
	static bool HandleDroppedFile(const std::string& diskPathUtf8, GameObject* selectedObject);

	// Pone la textura en el material del GameObject y de todos sus hijos con malla
	static bool AssignTextureToHierarchy(UID textureUID, GameObject* target);
};
