#include "MeshImporter.h"
#include "../../FileSystem/FileSystem.h"
#include "../../../Utils/Log.h"

#include <cstdint>
#include <cstring>

namespace
{
	constexpr char MESH_MAGIC[4] = { 'M', 'M', 'S', 'H' };
	constexpr uint32_t MESH_VERSION = 1;

	struct MeshFileHeader
	{
		char magic[4];
		uint32_t version;
		uint32_t vertexCount;
		uint32_t indexCount;
		float aabbMin[3];
		float aabbMax[3];
	};
	static_assert(sizeof(MeshFileHeader) == 40, "La cabecera .mesh debe ocupar 40 bytes");

	// El formato copia los vertices tal cual estan en memoria: Vertex debe ser 8 floats seguidos
	static_assert(sizeof(Vertex) == 8 * sizeof(float), "Vertex debe ser posicion(3) + normal(3) + uv(2) sin relleno");
}

bool MeshImporter::Save(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::string& libraryPath)
{
	if (vertices.empty() || indices.empty())
	{
		LOG_ERROR("[MeshImporter] Malla vacia, no se guarda '%s'", libraryPath.c_str());
		return false;
	}

	MeshFileHeader header{};
	std::memcpy(header.magic, MESH_MAGIC, sizeof(MESH_MAGIC));
	header.version = MESH_VERSION;
	header.vertexCount = static_cast<uint32_t>(vertices.size());
	header.indexCount = static_cast<uint32_t>(indices.size());

	glm::vec3 minPoint = vertices[0].position;
	glm::vec3 maxPoint = vertices[0].position;
	for (const Vertex& vertex : vertices)
	{
		minPoint = glm::min(minPoint, vertex.position);
		maxPoint = glm::max(maxPoint, vertex.position);
	}
	for (int i = 0; i < 3; ++i)
	{
		header.aabbMin[i] = minPoint[i];
		header.aabbMax[i] = maxPoint[i];
	}

	const size_t vertexBytes = vertices.size() * sizeof(Vertex);
	const size_t indexBytes = indices.size() * sizeof(uint32_t);

	// Un unico buffer -> una unica escritura a disco
	std::vector<char> buffer(sizeof(MeshFileHeader) + vertexBytes + indexBytes);
	char* cursor = buffer.data();
	std::memcpy(cursor, &header, sizeof(MeshFileHeader));
	cursor += sizeof(MeshFileHeader);
	std::memcpy(cursor, vertices.data(), vertexBytes);
	cursor += vertexBytes;
	for (unsigned int index : indices)
	{
		const uint32_t value = static_cast<uint32_t>(index);
		std::memcpy(cursor, &value, sizeof(uint32_t));
		cursor += sizeof(uint32_t);
	}

	return FileSystem::WriteBinary(libraryPath, buffer.data(), buffer.size());
}

std::unique_ptr<Primitive> MeshImporter::Load(const std::string& libraryPath)
{
	std::vector<char> data;
	if (!FileSystem::ReadBinary(libraryPath, data) || data.size() < sizeof(MeshFileHeader))
	{
		LOG_ERROR("[MeshImporter] No se pudo leer '%s'", libraryPath.c_str());
		return nullptr;
	}

	MeshFileHeader header{};
	std::memcpy(&header, data.data(), sizeof(MeshFileHeader));
	if (std::memcmp(header.magic, MESH_MAGIC, sizeof(MESH_MAGIC)) != 0 || header.version != MESH_VERSION)
	{
		LOG_ERROR("[MeshImporter] '%s' no es un fichero .mesh valido", libraryPath.c_str());
		return nullptr;
	}

	const size_t vertexBytes = static_cast<size_t>(header.vertexCount) * sizeof(Vertex);
	const size_t indexBytes = static_cast<size_t>(header.indexCount) * sizeof(uint32_t);
	if (header.vertexCount == 0 || header.indexCount == 0 || data.size() < sizeof(MeshFileHeader) + vertexBytes + indexBytes)
	{
		LOG_ERROR("[MeshImporter] '%s' esta vacio o incompleto", libraryPath.c_str());
		return nullptr;
	}

	const char* cursor = data.data() + sizeof(MeshFileHeader);

	std::vector<Vertex> vertices(header.vertexCount);
	std::memcpy(vertices.data(), cursor, vertexBytes);
	cursor += vertexBytes;

	std::vector<unsigned int> indices(header.indexCount);
	for (uint32_t i = 0; i < header.indexCount; ++i)
	{
		uint32_t value = 0;
		std::memcpy(&value, cursor, sizeof(uint32_t));
		cursor += sizeof(uint32_t);

		// Proteccion contra ficheros corruptos: un indice fuera de rango haria fallar a la GPU
		if (value >= header.vertexCount)
		{
			LOG_ERROR("[MeshImporter] Indice fuera de rango en '%s'", libraryPath.c_str());
			return nullptr;
		}
		indices[i] = value;
	}

	auto primitive = std::make_unique<Primitive>();
	primitive->Init(std::move(vertices), std::move(indices));
	return primitive;
}
