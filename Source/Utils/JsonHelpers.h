#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// Lectura y escritura de tipos de glm en JSON. Las lecturas son tolerantes:
// si la clave falta o tiene otro tipo, se conserva el valor anterior
namespace JsonHelpers
{
	inline nlohmann::json FromVec3(const glm::vec3& v) { return nlohmann::json::array({ v.x, v.y, v.z }); }
	inline nlohmann::json FromVec4(const glm::vec4& v) { return nlohmann::json::array({ v.x, v.y, v.z, v.w }); }

	// Lee 'count' números en 'out'; si el array no es válido no toca 'out'
	inline bool ReadFloatArray(const nlohmann::json& json, float* out, int count)
	{
		if (!json.is_array() || static_cast<int>(json.size()) != count) return false;
		for (int i = 0; i < count; ++i)
		{
			if (!json[i].is_number()) return false;
		}
		for (int i = 0; i < count; ++i)
		{
			out[i] = json[i].get<float>();
		}
		return true;
	}

	inline void ReadVec3(const nlohmann::json& object, const char* key, glm::vec3& value)
	{
		const auto it = object.find(key);
		if (it != object.end()) ReadFloatArray(*it, &value.x, 3);
	}

	inline void ReadVec4(const nlohmann::json& object, const char* key, glm::vec4& value)
	{
		const auto it = object.find(key);
		if (it != object.end()) ReadFloatArray(*it, &value.x, 4);
	}

	template <typename T>
	void Read(const nlohmann::json& object, const char* key, T& value)
	{
		const auto it = object.find(key);
		if (it == object.end()) return;
		try
		{
			value = it->get<T>();
		}
		catch (const nlohmann::json::exception&)
		{
			// Tipo incorrecto: se conserva el valor anterior
		}
	}

	// Como Read, pero devuelve 'fallback' si la clave falta o no es válida
	template <typename T>
	T Get(const nlohmann::json& object, const char* key, T fallback)
	{
		Read(object, key, fallback);
		return fallback;
	}

	inline glm::vec3 GetVec3(const nlohmann::json& object, const char* key, glm::vec3 fallback)
	{
		ReadVec3(object, key, fallback);
		return fallback;
	}

	inline glm::vec4 GetVec4(const nlohmann::json& object, const char* key, glm::vec4 fallback)
	{
		ReadVec4(object, key, fallback);
		return fallback;
	}
}
