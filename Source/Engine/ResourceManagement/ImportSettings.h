#pragma once

#include <nlohmann/json_fwd.hpp>

// Opciones de importación. Van en el .meta de cada asset para que Library se regenere igual

// ------------------------------- TEXTURAS -------------------------------------------
struct TextureImportSettings
{
	enum Filter { FILTER_NEAREST = 0, FILTER_LINEAR = 1, FILTER_TRILINEAR = 2 };
	enum Wrap { WRAP_REPEAT = 0, WRAP_CLAMP = 1, WRAP_MIRRORED = 2 };

	bool flipX = false;               // espejo horizontal de la imagen
	bool flipY = false;               // volteo vertical de la imagen
	int minFilter = FILTER_TRILINEAR; // filtro al alejarse (trilinear = usa mipmaps)
	int magFilter = FILTER_LINEAR;    // filtro al acercarse (solo nearest o linear)
	int wrapMode = WRAP_REPEAT;       // que pasa con las UV fuera de [0, 1]
	int maxSize = 2048;               // si la imagen es mas grande se reduce al importar

	void Save(nlohmann::json& out) const;
	void Load(const nlohmann::json& in);

	bool operator==(const TextureImportSettings& other) const;
	bool operator!=(const TextureImportSettings& other) const { return !(*this == other); }
};

// ------------------------------- MODELOS (FBX...) -----------------------------------
struct ModelImportSettings
{
	// Eje que en el fichero apunta hacia "arriba". Se gira todo el modelo para que pase a ser +Y.
	enum UpAxis { AXIS_Y_UP = 0, AXIS_Z_UP = 1, AXIS_X_UP = 2, AXIS_NEG_Z_UP = 3, AXIS_NEG_X_UP = 4, AXIS_NEG_Y_UP = 5 };

	float globalScale = 1.0f;     // escala extra aplicada a todo el modelo
	bool convertUnits = true;     // FBX: convierte las unidades del fichero a metros
	int upAxis = AXIS_Y_UP;       // eje que el fichero usa como "arriba" (se convierte a Y)
	bool mirrorX = false;         // espejo en X / Z (corrige el orden de los triangulos)
	bool mirrorZ = false;
	bool ignoreCameras = true;    // no crear GameObjects para las camaras del fichero
	bool ignoreLights = true;     // no crear GameObjects para las luces del fichero

	void Save(nlohmann::json& out) const;
	void Load(const nlohmann::json& in);

	bool operator==(const ModelImportSettings& other) const;
	bool operator!=(const ModelImportSettings& other) const { return !(*this == other); }
};
