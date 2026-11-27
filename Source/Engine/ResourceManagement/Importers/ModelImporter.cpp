#include "ModelImporter.h"
#include "MeshImporter.h"
#include "../../FileSystem/FileSystem.h"
#include "../../../Utils/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <nlohmann/json.hpp>

#include <cctype>
#include <cmath>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace
{
	constexpr int MODEL_FILE_VERSION = 1;

	// Datos del material que nos interesan: textura difusa y color
	struct MaterialInfo
	{
		UID texture = INVALID_UID;
		glm::vec4 color = glm::vec4(1.0f);
	};

	// Assimp guarda las matrices por filas y GLM por columnas
	glm::mat4 ToGlm(const aiMatrix4x4& m)
	{
		return glm::mat4(m.a1, m.b1, m.c1, m.d1,  // columna 0
			m.a2, m.b2, m.c2, m.d2,                // columna 1
			m.a3, m.b3, m.c3, m.d3,                // columna 2
			m.a4, m.b4, m.c4, m.d4);               // columna 3 (traslacion)
	}

	// Separa una matriz en posicion, rotacion (Euler XYZ en grados, el formato del
	// TransformComponent) y escala
	void Decompose(const glm::mat4& matrix, glm::vec3& position, glm::vec3& rotationDegrees, glm::vec3& scale)
	{
		position = glm::vec3(matrix[3]);

		const glm::vec3 axisX(matrix[0]);
		const glm::vec3 axisY(matrix[1]);
		const glm::vec3 axisZ(matrix[2]);
		scale = glm::vec3(glm::length(axisX), glm::length(axisY), glm::length(axisZ));

		// Si el determinante es negativo el modelo esta "espejado": lo representamos con escala X negativa
		if (glm::dot(glm::cross(axisX, axisY), axisZ) < 0.0f) scale.x = -scale.x;

		glm::mat4 rotation(1.0f);
		if (std::abs(scale.x) > 1e-8f) rotation[0] = glm::vec4(axisX / scale.x, 0.0f);
		if (std::abs(scale.y) > 1e-8f) rotation[1] = glm::vec4(axisY / scale.y, 0.0f);
		if (std::abs(scale.z) > 1e-8f) rotation[2] = glm::vec4(axisZ / scale.z, 0.0f);

		float x = 0.0f, y = 0.0f, z = 0.0f;
		glm::extractEulerAngleXYZ(rotation, x, y, z);
		rotationDegrees = glm::degrees(glm::vec3(x, y, z));
	}

	// Copia una malla de Assimp a nuestro formato. 'mirror' es diagonal (+-1): si invierte la
	// orientacion se cambia el orden de los triangulos para que no se vean del reves
	bool ExtractMesh(const aiMesh* mesh, const glm::vec3& mirror, std::vector<Vertex>& vertices, std::vector<unsigned int>& indices)
	{
		if (mesh == nullptr || mesh->mVertices == nullptr || mesh->mNumVertices == 0) return false;
		if ((mesh->mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0) return false;

		vertices.resize(mesh->mNumVertices);
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
			Vertex& vertex = vertices[i];
			vertex.position = mirror * glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

			vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
			if (mesh->mNormals != nullptr)
			{
				const glm::vec3 normal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
				const float length = glm::length(normal);
				if (length > 1e-12f) vertex.normal = mirror * (normal / length); // descarta normales nulas o NaN
			}

			vertex.texCoords = glm::vec2(0.0f);
			if (mesh->mTextureCoords[0] != nullptr)
			{
				vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
			}
		}

		const bool flipWinding = mirror.x * mirror.y * mirror.z < 0.0f;
		indices.clear();
		indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3) continue; // solo triangulos
			indices.push_back(face.mIndices[0]);
			indices.push_back(face.mIndices[flipWinding ? 2 : 1]);
			indices.push_back(face.mIndices[flipWinding ? 1 : 2]);
		}
		return !indices.empty();
	}

	bool EndsWith(const std::string& text, const std::string& suffix)
	{
		return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
	}

	// Recorre la jerarquia de Assimp y crea la lista plana de ModelNode (padres antes que hijos)
	class HierarchyBuilder
	{
	public:
		HierarchyBuilder(const aiScene& scene, const ModelImportSettings& settings, const glm::mat4& mirror, const std::vector<UID>& meshUIDs,
			const std::vector<std::string>& meshNames, const std::vector<MaterialInfo>& materials, std::vector<ModelNode>& nodes)
			: scene(scene), settings(settings), mirror(mirror), meshUIDs(meshUIDs), meshNames(meshNames), materials(materials), nodes(nodes)
		{
			for (unsigned int i = 0; i < scene.mNumCameras; ++i) cameraNames.insert(scene.mCameras[i]->mName.C_Str());
			for (unsigned int i = 0; i < scene.mNumLights; ++i) lightNames.insert(scene.mLights[i]->mName.C_Str());
		}

		void Visit(const aiNode* node, int parentIndex, const glm::mat4& localMatrix, const std::string& nameOverride)
		{
			const std::string nodeName = node->mName.length > 0 ? node->mName.C_Str() : "Node";

			// 3ds Max crea nodos "Camera001.Target" / "Spot001.Target" para orientar camaras y luces
			std::string baseName = nodeName;
			if (EndsWith(baseName, ".Target")) baseName.resize(baseName.size() - 7);
			const bool isCamera = cameraNames.count(baseName) > 0;
			const bool isLight = lightNames.count(baseName) > 0;

			if ((isCamera && settings.ignoreCameras) || (isLight && settings.ignoreLights))
			{
				// Opcion "ignorar camaras / luces": si el nodo no tiene nada colgando, no se importa
				if (node->mNumMeshes == 0 && node->mNumChildren == 0) return;
			}

			ModelNode modelNode;
			modelNode.name = nameOverride.empty() ? nodeName : nameOverride;
			modelNode.parent = parentIndex;
			modelNode.isCamera = isCamera && !settings.ignoreCameras && baseName == nodeName;
			Decompose(localMatrix, modelNode.position, modelNode.rotation, modelNode.scale);

			const int myIndex = static_cast<int>(nodes.size());
			nodes.push_back(modelNode);

			// Mallas del nodo: si solo hay una va en el propio nodo; si hay varias, un hijo por malla
			std::vector<unsigned int> validMeshes;
			for (unsigned int i = 0; i < node->mNumMeshes; ++i)
			{
				const unsigned int meshIndex = node->mMeshes[i];
				if (meshIndex < meshUIDs.size() && meshUIDs[meshIndex] != INVALID_UID) validMeshes.push_back(meshIndex);
			}

			if (validMeshes.size() == 1)
			{
				AssignMesh(nodes[myIndex], validMeshes[0]);
			}
			else
			{
				for (size_t i = 0; i < validMeshes.size(); ++i)
				{
					ModelNode child;
					const std::string& meshName = meshNames[validMeshes[i]];
					child.name = meshName.empty() ? (nodes[myIndex].name + "_" + std::to_string(i)) : meshName;
					child.parent = myIndex;
					AssignMesh(child, validMeshes[i]);
					nodes.push_back(child);
				}
			}

			for (unsigned int c = 0; c < node->mNumChildren; ++c)
			{
				const aiNode* child = node->mChildren[c];
				// M*L*M: con los vertices ya espejados el resultado es el del modelo espejado entero
				Visit(child, myIndex, mirror * ToGlm(child->mTransformation) * mirror, std::string());
			}
		}

	private:
		void AssignMesh(ModelNode& node, unsigned int meshIndex) const
		{
			node.mesh = meshUIDs[meshIndex];
			const unsigned int materialIndex = scene.mMeshes[meshIndex]->mMaterialIndex;
			if (materialIndex < materials.size())
			{
				node.texture = materials[materialIndex].texture;
				node.color = materials[materialIndex].color;
			}
		}

		const aiScene& scene;
		const ModelImportSettings& settings;
		const glm::mat4 mirror;
		const std::vector<UID>& meshUIDs;
		const std::vector<std::string>& meshNames;
		const std::vector<MaterialInfo>& materials;
		std::vector<ModelNode>& nodes;
		std::unordered_set<std::string> cameraNames;
		std::unordered_set<std::string> lightNames;
	};

	// Factor de unidades del FBX: "UnitScaleFactor" son los centimetros por unidad (100 = metros)
	float GetUnitScaleToMeters(const aiScene& scene)
	{
		if (scene.mMetaData == nullptr) return 1.0f;

		const std::string key = "UnitScaleFactor";

		// Segun el exportador viene guardado como double o como float
		double factor = 0.0;
		if (scene.mMetaData->Get(key, factor) && factor > 0.0) return static_cast<float>(factor / 100.0);

		float factorF = 0.0f;
		if (scene.mMetaData->Get(key, factorF) && factorF > 0.0f) return factorF / 100.0f;
		return 1.0f;
	}

	nlohmann::json Vec3ToJson(const glm::vec3& v) { return nlohmann::json::array({ v.x, v.y, v.z }); }
	nlohmann::json Vec4ToJson(const glm::vec4& v) { return nlohmann::json::array({ v.x, v.y, v.z, v.w }); }

	void ReadFloats(const nlohmann::json& node, const char* key, float* out, int count)
	{
		const auto it = node.find(key);
		if (it == node.end() || !it->is_array() || static_cast<int>(it->size()) != count) return;
		for (int i = 0; i < count; ++i)
		{
			if ((*it)[i].is_number()) out[i] = (*it)[i].get<float>();
		}
	}
}

bool ModelImporter::IsSupportedExtension(const std::string& extension)
{
	const std::string ext = FileSystem::ToLower(extension);
	return ext == ".fbx" || ext == ".obj" || ext == ".dae" || ext == ".3ds" || ext == ".gltf" || ext == ".glb";
}

bool ModelImporter::Import(const std::string& assetPath, const ModelImportSettings& settings, const std::string& modelLibraryPath,
	std::vector<MeshEntry>& inOutMeshes, const TextureResolver& resolveTexture, const EmbeddedTextureHandler& handleEmbeddedTexture)
{
	Assimp::Importer importer;
	// Quita los nodos auxiliares "$AssimpFbx$_..." que Assimp crea para los pivotes de FBX
	importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
	// Elimina puntos y lineas sueltas: solo queremos triangulos
	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

	const unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType | aiProcess_ImproveCacheLocality;

	const std::string diskPath = FileSystem::ToDiskPath(assetPath);
	const aiScene* scene = importer.ReadFile(diskPath, flags);
	if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mRootNode == nullptr)
	{
		LOG_ERROR("[ModelImporter] Assimp no pudo leer '%s': %s", assetPath.c_str(), importer.GetErrorString());
		return false;
	}

	const std::string modelName = FileSystem::GetFileNameNoExtension(assetPath);

	// ---------------------------------------------------------------------------------
	// 1) MALLAS -> un fichero .mesh por malla. Se reutiliza el UID que tenia en el .meta
	// ---------------------------------------------------------------------------------
	std::unordered_map<int, UID> previousUIDs;
	for (const MeshEntry& entry : inOutMeshes) previousUIDs[entry.index] = entry.uid;

	std::vector<UID> meshUIDs(scene->mNumMeshes, INVALID_UID);
	std::vector<std::string> meshNames(scene->mNumMeshes);
	std::vector<MeshEntry> newEntries;

	// Nombre de cada malla: el del nodo que la usa (suele ser mas descriptivo, p.ej. "Chimney"
	// en vez de "Untitled.001"). Si la usa el nodo raiz, el nombre del modelo.
	std::vector<std::string> nodeNameForMesh(scene->mNumMeshes);
	std::vector<const aiNode*> pendingNodes = { scene->mRootNode };
	while (!pendingNodes.empty())
	{
		const aiNode* node = pendingNodes.back();
		pendingNodes.pop_back();
		const std::string nodeName = (node == scene->mRootNode) ? modelName : std::string(node->mName.C_Str());
		for (unsigned int m = 0; m < node->mNumMeshes; ++m)
		{
			const unsigned int meshIndex = node->mMeshes[m];
			if (meshIndex < scene->mNumMeshes && nodeNameForMesh[meshIndex].empty()) nodeNameForMesh[meshIndex] = nodeName;
		}
		for (unsigned int c = 0; c < node->mNumChildren; ++c) pendingNodes.push_back(node->mChildren[c]);
	}
	std::unordered_map<std::string, int> usedMeshNames; // para no repetir nombres: "Malla", "Malla (1)"...

	const glm::vec3 mirror(settings.mirrorX ? -1.0f : 1.0f, 1.0f, settings.mirrorZ ? -1.0f : 1.0f);
	const glm::mat4 mirrorMatrix = glm::scale(glm::mat4(1.0f), mirror);

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		if (!ExtractMesh(mesh, mirror, vertices, indices)) continue;

		const auto previous = previousUIDs.find(static_cast<int>(i));
		const UID uid = (previous != previousUIDs.end() && previous->second != INVALID_UID) ? previous->second : GenerateUID();

		std::string name = !nodeNameForMesh[i].empty() ? nodeNameForMesh[i] :
			(mesh->mName.length > 0 ? std::string(mesh->mName.C_Str()) : modelName + "_Mesh" + std::to_string(i));
		int& repetitions = usedMeshNames[name];
		if (repetitions > 0) name += " (" + std::to_string(repetitions) + ")";
		++repetitions;

		if (!MeshImporter::Save(vertices, indices, GetLibraryPathFor(ResourceType::Mesh, uid)))
		{
			LOG_ERROR("[ModelImporter] No se pudo guardar la malla %u de '%s'", i, assetPath.c_str());
			continue;
		}

		meshUIDs[i] = uid;
		meshNames[i] = name;
		newEntries.push_back({ static_cast<int>(i), uid, name });
	}

	// ---------------------------------------------------------------------------------
	// 2) MATERIALES -> textura difusa (se busca entre los assets) y color
	// ---------------------------------------------------------------------------------
	std::vector<MaterialInfo> materials(scene->mNumMaterials);
	for (unsigned int m = 0; m < scene->mNumMaterials; ++m)
	{
		const aiMaterial* material = scene->mMaterials[m];
		MaterialInfo& info = materials[m];

		aiString texturePath;
		const bool hasTexture = material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS ||
			material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) == AI_SUCCESS;

		if (hasTexture && texturePath.length > 0)
		{
			const aiTexture* embedded = scene->GetEmbeddedTexture(texturePath.C_Str());
			if (embedded != nullptr)
			{
				// Textura guardada DENTRO del modelo: solo soportamos las comprimidas (png/jpg/tga...)
				if (embedded->mHeight == 0 && embedded->pcData != nullptr && handleEmbeddedTexture)
				{
					// Mismo nombre de siempre: <modelo>_embedded<clave saneada>.<ext>, para no duplicar las ya extraidas
					std::string key = texturePath.C_Str();
					for (char& c : key) if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
					std::string ext = FileSystem::ToLower(embedded->achFormatHint);
					if (ext.empty()) ext = "png";
					if (ext == "jpeg") ext = "jpg";
					info.texture = handleEmbeddedTexture(modelName + "_embedded" + key + "." + ext, embedded->pcData, embedded->mWidth);
				}
				else
				{
					LOG_WARNING("[ModelImporter] Textura embebida sin comprimir en '%s': no soportada", assetPath.c_str());
				}
			}
			else if (resolveTexture)
			{
				info.texture = resolveTexture(texturePath.C_Str());
			}
		}

		// Sin textura usamos el color difuso del material (por ejemplo el cesped verde de la calle)
		if (info.texture == INVALID_UID)
		{
			aiColor4D diffuse;
			if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
			{
				info.color = glm::vec4(diffuse.r, diffuse.g, diffuse.b, 1.0f);
			}
		}
	}

	// ---------------------------------------------------------------------------------
	// 3) JERARQUIA -> lista de ModelNode. Las opciones de importacion (escala, unidades y
	//    eje vertical) se aplican al transform del nodo raiz.
	// ---------------------------------------------------------------------------------
	float scaleFactor = settings.globalScale;
	if (settings.convertUnits) scaleFactor *= GetUnitScaleToMeters(*scene);

	// Giro que lleva el eje "arriba" del fichero al eje +Y del motor
	const glm::vec3 axisX(1.0f, 0.0f, 0.0f);
	const glm::vec3 axisZ(0.0f, 0.0f, 1.0f);
	glm::mat4 axisCorrection(1.0f);
	switch (settings.upAxis)
	{
	case ModelImportSettings::AXIS_Z_UP:     axisCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), axisX); break;  // +Z -> +Y
	case ModelImportSettings::AXIS_X_UP:     axisCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), axisZ); break;   // +X -> +Y
	case ModelImportSettings::AXIS_NEG_Z_UP: axisCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), axisX); break;   // -Z -> +Y
	case ModelImportSettings::AXIS_NEG_X_UP: axisCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), axisZ); break;  // -X -> +Y
	case ModelImportSettings::AXIS_NEG_Y_UP: axisCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), axisX); break;  // -Y -> +Y
	default: break;                                                                                                              // +Y: sin cambios
	}
	const glm::mat4 rootMatrix = axisCorrection * glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor)) *
		mirrorMatrix * ToGlm(scene->mRootNode->mTransformation) * mirrorMatrix;

	std::vector<ModelNode> nodes;
	HierarchyBuilder builder(*scene, settings, mirrorMatrix, meshUIDs, meshNames, materials, nodes);
	builder.Visit(scene->mRootNode, -1, rootMatrix, modelName);

	// ---------------------------------------------------------------------------------
	// 4) Fichero .model (JSON)
	// ---------------------------------------------------------------------------------
	nlohmann::json root;
	root["version"] = MODEL_FILE_VERSION;
	root["source"] = assetPath;
	nlohmann::json& jsonNodes = root["nodes"];
	jsonNodes = nlohmann::json::array();
	for (const ModelNode& node : nodes)
	{
		nlohmann::json jsonNode;
		jsonNode["name"] = node.name;
		jsonNode["parent"] = node.parent;
		jsonNode["position"] = Vec3ToJson(node.position);
		jsonNode["rotation"] = Vec3ToJson(node.rotation);
		jsonNode["scale"] = Vec3ToJson(node.scale);
		jsonNode["mesh"] = node.mesh;
		jsonNode["texture"] = node.texture;
		jsonNode["color"] = Vec4ToJson(node.color);
		jsonNode["camera"] = node.isCamera;
		jsonNodes.push_back(std::move(jsonNode));
	}

	std::string text;
	try
	{
		// Un nombre de nodo con caracteres raros (UTF-8 invalido) no debe romper la importacion
		text = root.dump(1, '\t', false, nlohmann::json::error_handler_t::replace);
	}
	catch (const nlohmann::json::exception& e)
	{
		LOG_ERROR("[ModelImporter] Error generando el .model de '%s': %s", assetPath.c_str(), e.what());
		return false;
	}

	if (!FileSystem::WriteText(modelLibraryPath, text))
	{
		LOG_ERROR("[ModelImporter] No se pudo escribir '%s'", modelLibraryPath.c_str());
		return false;
	}

	inOutMeshes = std::move(newEntries);

	unsigned int triangles = 0;
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i) triangles += scene->mMeshes[i]->mNumFaces;
	LOG("[ModelImporter] '%s' importado: %zu nodos, %zu mallas, %u materiales, %u triangulos (escala %.3f)",
		assetPath.c_str(), nodes.size(), inOutMeshes.size(), scene->mNumMaterials, triangles, scaleFactor);
	return true;
}

bool ModelImporter::LoadModelFile(const std::string& libraryPath, std::vector<ModelNode>& outNodes)
{
	outNodes.clear();

	std::string text;
	if (!FileSystem::ReadText(libraryPath, text)) return false;

	const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object()) return false;

	const auto nodesIt = root.find("nodes");
	if (nodesIt == root.end() || !nodesIt->is_array()) return false;

	try
	{
		outNodes.reserve(nodesIt->size());
		for (const nlohmann::json& jsonNode : *nodesIt)
		{
			ModelNode node;
			node.name = jsonNode.value("name", std::string("Node"));
			node.parent = jsonNode.value("parent", -1);
			node.mesh = jsonNode.value("mesh", static_cast<UID>(INVALID_UID));
			node.texture = jsonNode.value("texture", static_cast<UID>(INVALID_UID));
			node.isCamera = jsonNode.value("camera", false);
			ReadFloats(jsonNode, "position", &node.position.x, 3);
			ReadFloats(jsonNode, "rotation", &node.rotation.x, 3);
			ReadFloats(jsonNode, "scale", &node.scale.x, 3);
			ReadFloats(jsonNode, "color", &node.color.x, 4);

			// El padre siempre tiene que aparecer antes que el hijo
			const int myIndex = static_cast<int>(outNodes.size());
			if (node.parent >= myIndex) node.parent = (myIndex > 0) ? 0 : -1;
			if (myIndex > 0 && node.parent < 0) node.parent = 0;

			outNodes.push_back(std::move(node));
		}
	}
	catch (const nlohmann::json::exception& e)
	{
		LOG_ERROR("[ModelImporter] '%s' tiene un formato incorrecto: %s", libraryPath.c_str(), e.what());
		outNodes.clear();
		return false;
	}

	return !outNodes.empty();
}

std::vector<std::string> ModelImporter::FindExternalTextureFiles(const std::string& modelDiskPathUtf8)
{
	namespace fs = std::filesystem;
	std::vector<std::string> result;

	try
	{
		const fs::path modelPath = fs::u8path(modelDiskPathUtf8);
		const fs::path folder = modelPath.parent_path();

		// Ficheros de la carpeta del modelo y de sus subcarpetas Texture(s), por nombre en minusculas
		std::unordered_map<std::string, fs::path> filesInFolder;
		std::vector<fs::path> textureFolders;
		std::error_code ec;
		for (const fs::directory_entry& entry : fs::directory_iterator(folder, ec))
		{
			std::error_code typeError;
			if (entry.is_directory(typeError))
			{
				const std::string name = FileSystem::ToLower(entry.path().filename().u8string());
				if (name == "texture" || name == "textures") textureFolders.push_back(entry.path());
				continue;
			}
			if (!entry.is_regular_file(typeError)) continue;
			filesInFolder[FileSystem::ToLower(entry.path().filename().u8string())] = entry.path();
		}
		for (const fs::path& textureFolder : textureFolders)
		{
			for (const fs::directory_entry& entry : fs::directory_iterator(textureFolder, ec))
			{
				std::error_code typeError;
				if (entry.is_regular_file(typeError)) filesInFolder.emplace(FileSystem::ToLower(entry.path().filename().u8string()), entry.path());
			}
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(modelDiskPathUtf8, 0); // sin post-proceso: solo queremos los materiales
		if (scene == nullptr) return result;

		std::unordered_set<std::string> added;
		for (unsigned int m = 0; m < scene->mNumMaterials; ++m)
		{
			const aiTextureType types[] = { aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR };
			for (aiTextureType type : types)
			{
				aiString texturePath;
				if (scene->mMaterials[m]->GetTexture(type, 0, &texturePath) != AI_SUCCESS) continue;

				const std::string fileName = FileSystem::ToLower(FileSystem::GetFileName(texturePath.C_Str()));
				const auto found = filesInFolder.find(fileName);
				if (found != filesInFolder.end() && added.insert(fileName).second)
				{
					result.push_back(found->second.u8string());
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		LOG_WARNING("[ModelImporter] No se pudieron buscar las texturas de '%s': %s", modelDiskPathUtf8.c_str(), e.what());
	}

	return result;
}
