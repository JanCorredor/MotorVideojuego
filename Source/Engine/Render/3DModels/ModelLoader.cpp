#include "ModelLoader.h"
#include "../../../Utils/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/version.h>

std::unique_ptr<Primitive> ModelLoader::LoadFBX(const std::string& path) 
{
    LOG("[Assimp] Iniciando carga de geometría: %s (Assimp v%u.%u.%u)",
        path.c_str(), aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionRevision());

    Assimp::Importer importer;

    // Post-procesado para triangular caras y calcular normales/tangentes
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("[Assimp] Error crítico al cargar modelo '%s': %s", path.c_str(), importer.GetErrorString());
        return nullptr;
    }

    LOG("[Assimp] Archivo '%s' procesado. Mallas encontradas: %u, Materiales: %u, Texturas embebidas: %u",
        path.c_str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumTextures);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Recorremos las mallas del archivo
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        unsigned int vertexOffset = static_cast<unsigned int>(vertices.size());

        const char* meshName = (mesh->mName.length > 0) ? mesh->mName.C_Str() : "SinNombre";
        LOG("[Assimp] Procesando sub-malla [%u/%u] '%s': %u vértices, %u caras (Normales: %s, UVs: %s)",
            i + 1, scene->mNumMeshes, meshName, mesh->mNumVertices, mesh->mNumFaces,
            mesh->HasNormals() ? "Sí" : "No",
            mesh->HasTextureCoords(0) ? "Sí" : "No");

        // Extraer vértices
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            Vertex v;
            v.position = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

            if (mesh->HasNormals()) {
                v.normal = glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            } else {
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (mesh->mTextureCoords[0]) {
                v.texCoords = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
            } else {
                v.texCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(v);
        }

        // Extraer índices
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                indices.push_back(vertexOffset + face.mIndices[k]);
            }
        }
    }

    LOG("[Assimp] Geometría cargada con éxito: %s (%zu vértices totales, %zu índices)", path.c_str(), vertices.size(), indices.size());

    auto primitive = std::make_unique<Primitive>();
    primitive->Init(std::move(vertices), std::move(indices));
    return primitive;
}

std::vector<SubMeshData> ModelLoader::LoadFBXMeshes(const std::string& path)
{
    std::vector<SubMeshData> result;

    LOG("[Assimp] Iniciando carga de sub-mallas: %s (Assimp v%u.%u.%u)",
        path.c_str(), aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionRevision());

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("[Assimp] Error crítico al cargar modelo '%s': %s", path.c_str(), importer.GetErrorString());
        return result;
    }

    LOG("[Assimp] Archivo '%s' procesado. Partes/Mallas encontradas: %u", path.c_str(), scene->mNumMeshes);

    std::vector<std::string> meshNodeNames(scene->mNumMeshes, "");
    std::function<void(const aiNode*)> traverseNodes = [&](const aiNode* node) {
        if (!node) return;
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            unsigned int meshIdx = node->mMeshes[i];
            if (meshIdx < scene->mNumMeshes && meshNodeNames[meshIdx].empty() && node->mName.length > 0) {
                meshNodeNames[meshIdx] = node->mName.C_Str();
            }
        }
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            traverseNodes(node->mChildren[i]);
        }
        };
    traverseNodes(scene->mRootNode);

    result.reserve(scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];

        std::string partName;
        if (!meshNodeNames[i].empty() && meshNodeNames[i] != "RootNode") {
            partName = meshNodeNames[i];
        }
        else if (mesh->mName.length > 0) {
            partName = mesh->mName.C_Str();
        }
        else {
            partName = "Part_" + std::to_string(i + 1);
        }

        LOG("[Assimp] Creando parte [%u/%u] '%s': %u vértices, %u caras",
            i + 1, scene->mNumMeshes, partName.c_str(), mesh->mNumVertices, mesh->mNumFaces);

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(mesh->mNumVertices);
        indices.reserve(mesh->mNumFaces * 3);

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            Vertex v;
            v.position = glm::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

            if (mesh->HasNormals()) {
                v.normal = glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            }
            else {
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (mesh->mTextureCoords[0]) {
                v.texCoords = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
            }
            else {
                v.texCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(v);
        }

        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                indices.push_back(face.mIndices[k]);
            }
        }

        auto primitive = std::make_unique<Primitive>();
        primitive->Init(std::move(vertices), std::move(indices));

        SubMeshData subMesh;
        subMesh.name = partName;
        subMesh.primitive = std::move(primitive);
        result.push_back(std::move(subMesh));
    }

    return result;
}