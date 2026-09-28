#include "ModelLoader.h"
#include "../../../Utils/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

std::unique_ptr<Primitive> ModelLoader::LoadFBX(const std::string& path) 
{
    Assimp::Importer importer;

    // Post-procesado para triangular caras y calcular normales/tangentes
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG("Error al cargar modelo con Assimp: %s", importer.GetErrorString());
        return nullptr;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Recorremos las mallas del archivo
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        unsigned int vertexOffset = static_cast<unsigned int>(vertices.size());

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

    LOG("Modelo cargado con éxito: %s (%zu vértices, %zu índices)", path.c_str(), vertices.size(), indices.size());

    auto primitive = std::make_unique<Primitive>();
    primitive->Init(vertices, indices);
    return primitive;
}