#include "ImportSettings.h"

#include <nlohmann/json.hpp>
#include <algorithm>

namespace
{
	// Lee un valor del JSON si existe y tiene el tipo correcto; si no, deja el valor por defecto
	template <typename T>
	void ReadValue(const nlohmann::json& in, const char* key, T& value)
	{
		const auto it = in.find(key);
		if (it == in.end()) return;
		try
		{
			value = it->get<T>();
		}
		catch (const nlohmann::json::exception&)
		{
			// Tipo incorrecto en el .meta: se ignora y se conserva el valor por defecto
		}
	}
}

// ------------------------------- TEXTURAS -------------------------------------------
void TextureImportSettings::Save(nlohmann::json& out) const
{
	out["flipX"] = flipX;
	out["flipY"] = flipY;
	out["minFilter"] = minFilter;
	out["magFilter"] = magFilter;
	out["wrapMode"] = wrapMode;
	out["maxSize"] = maxSize;
}

void TextureImportSettings::Load(const nlohmann::json& in)
{
	if (!in.is_object()) return;
	ReadValue(in, "flipX", flipX);
	ReadValue(in, "flipY", flipY);
	ReadValue(in, "minFilter", minFilter);
	ReadValue(in, "magFilter", magFilter);
	ReadValue(in, "wrapMode", wrapMode);
	ReadValue(in, "maxSize", maxSize);

	// Valores fuera de rango (por si alguien edita el .meta a mano)
	minFilter = std::clamp(minFilter, (int)FILTER_NEAREST, (int)FILTER_TRILINEAR);
	magFilter = std::clamp(magFilter, (int)FILTER_NEAREST, (int)FILTER_LINEAR);
	wrapMode = std::clamp(wrapMode, (int)WRAP_REPEAT, (int)WRAP_MIRRORED);
	maxSize = std::clamp(maxSize, 32, 16384);
}

bool TextureImportSettings::operator==(const TextureImportSettings& other) const
{
	return flipX == other.flipX && flipY == other.flipY && minFilter == other.minFilter &&
		magFilter == other.magFilter && wrapMode == other.wrapMode && maxSize == other.maxSize;
}

// ------------------------------- MODELOS --------------------------------------------
void ModelImportSettings::Save(nlohmann::json& out) const
{
	out["globalScale"] = globalScale;
	out["convertUnits"] = convertUnits;
	out["upAxis"] = upAxis;
	out["mirrorX"] = mirrorX;
	out["mirrorZ"] = mirrorZ;
	out["ignoreCameras"] = ignoreCameras;
	out["ignoreLights"] = ignoreLights;
}

void ModelImportSettings::Load(const nlohmann::json& in)
{
	if (!in.is_object()) return;
	ReadValue(in, "globalScale", globalScale);
	ReadValue(in, "convertUnits", convertUnits);
	ReadValue(in, "upAxis", upAxis);
	ReadValue(in, "mirrorX", mirrorX);
	ReadValue(in, "mirrorZ", mirrorZ);
	ReadValue(in, "ignoreCameras", ignoreCameras);
	ReadValue(in, "ignoreLights", ignoreLights);

	if (!(globalScale > 0.0f)) globalScale = 1.0f; // tambien descarta NaN
	upAxis = std::clamp(upAxis, (int)AXIS_Y_UP, (int)AXIS_NEG_Y_UP);
}

bool ModelImportSettings::operator==(const ModelImportSettings& other) const
{
	return globalScale == other.globalScale && convertUnits == other.convertUnits && upAxis == other.upAxis &&
		ignoreCameras == other.ignoreCameras && ignoreLights == other.ignoreLights &&
		mirrorX == other.mirrorX && mirrorZ == other.mirrorZ;
}
