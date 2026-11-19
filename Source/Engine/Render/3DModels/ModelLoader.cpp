#include "ModelLoader.h"
#include "../../../Utils/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/version.h>
#include <stack>

// Convierte una matriz de Assimp (almacenada por filas / row-major)
// a una matriz de GLM (almacenada por columnas / column-major)
glm::mat4 ModelLoader::AiToGlm(const aiMatrix4x4& m)
{
    glm::mat4 res(1.0f);
    res[0][0] = m.a1; res[1][0] = m.a2; res[2][0] = m.a3; res[3][0] = m.a4;
    res[0][1] = m.b1; res[1][1] = m.b2; res[2][1] = m.b3; res[3][1] = m.b4;
    res[0][2] = m.c1; res[1][2] = m.c2; res[2][2] = m.c3; res[3][2] = m.c4;
    res[0][3] = m.d1; res[1][3] = m.d2; res[2][3] = m.d3; res[3][3] = m.d4;
    return res;
}

// Aplica la reorientación de ejes configurada en MeshImportSettings y escala los vértices
void ModelLoader::ApplyAxisAndScale(glm::vec3& pos, glm::vec3& normal, const MeshImportSettings& settings)
{
    switch (settings.axisPreset)
    {
    case 1: // Z-Up -> Y-Up (+X, +Z, -Y)
        pos = glm::vec3(pos.x, pos.z, -pos.y);
        normal = glm::vec3(normal.x, normal.z, -normal.y);
        break;
    case 2: // Flip Z (+X, +Y, -Z)
        pos.z = -pos.z;
        normal.z = -normal.z;
        break;
    case 3: // Flip X (-X, +Y, +Z)
        pos.x = -pos.x;
        normal.x = -normal.x;
        break;
    default: // Standard (+X, +Y, +Z)
        break;
    }

    // Escalar posición y re-normalizar el vector normal
    pos *= settings.globalScale;
    float nLen = glm::length(normal);
    if (nLen > 1e-6f) normal /= nLen;
}

// Devuelve true si el preset de ejes produce una reflexión especular (determinante negativo)
bool ModelLoader::IsAxisMirrored(int axisPreset)
{
    return (axisPreset == 2 || axisPreset == 3);
}

// Obtiene un conjunto con los nombres de nodos que representan cámaras o luces embebidas en el FBX
std::unordered_set<std::string> ModelLoader::CollectIgnoredNodeNames(const aiScene* scene, const MeshImportSettings& settings)
{
    std::unordered_set<std::string> ignored;
    if (!scene) return ignored;

    // Registrar nombres de cámaras si está activado ignoreCameras
    if (settings.ignoreCameras && scene->HasCameras())
    {
        for (unsigned int c = 0; c < scene->mNumCameras; ++c)
        {
            if (scene->mCameras[c] && scene->mCameras[c]->mName.length > 0)
            {
                ignored.insert(scene->mCameras[c]->mName.C_Str());
            }
        }
    }

    // Registrar nombres de luces si está activado ignoreLights
    if (settings.ignoreLights && scene->HasLights())
    {
        for (unsigned int l = 0; l < scene->mNumLights; ++l)
        {
            if (scene->mLights[l] && scene->mLights[l]->mName.length > 0)
            {
                ignored.insert(scene->mLights[l]->mName.C_Str());
            }
        }
    }
    return ignored;
}


void ModelLoader::ProcessNodeMerged(
    const aiNode* node,
    const aiScene* scene,
    const glm::mat4& parentTransform,
    const std::unordered_set<std::string>& ignoredNodes,
    const MeshImportSettings& settings,
    std::vector<Vertex>& outVertices,
    std::vector<unsigned int>& outIndices)
{
    if (!node) return;

    // Si el nodo pertenece a una cámara o luz ignorada, descartamos esta rama
    if (node->mName.length > 0 && ignoredNodes.count(node->mName.C_Str()) > 0)
    {
        return;
    }

    // Calcular matriz acumulada del nodo en espacio de escena (incluye pivotes y offsets de 3ds Max)
    glm::mat4 worldTransform = parentTransform * AiToGlm(node->mTransformation);

    // Matriz normal (inversa transpuesta de la parte 3x3) para transformar normales correctamente
    // incluso si el nodo tiene escalado no uniforme
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

    // Detectar si el nodo tiene escala negativa (espejo) o si el preset de ejes invierte el espacio.
    bool nodeMirrored = glm::determinant(glm::mat3(worldTransform)) < 0.0f;
    bool flipWinding = IsAxisMirrored(settings.axisPreset) ^ nodeMirrored;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        unsigned int meshIdx = node->mMeshes[i];
        if (meshIdx >= scene->mNumMeshes) continue;

        const aiMesh* mesh = scene->mMeshes[meshIdx];
        unsigned int vertexOffset = static_cast<unsigned int>(outVertices.size());

        const char* meshName = (mesh->mName.length > 0) ? mesh->mName.C_Str() : "SinNombre";
        LOG("[Assimp] Procesando sub-malla [%u/%u] '%s': %u vértices, %u caras (Normales: %s, UVs: %s)",
            meshIdx + 1, scene->mNumMeshes, meshName, mesh->mNumVertices, mesh->mNumFaces,
            mesh->HasNormals() ? "Sí" : "No",
            mesh->HasTextureCoords(0) ? "Sí" : "No");

        // Extraer y transformar vértices de la malla al espacio del nodo
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            Vertex v;
            glm::vec4 worldPos = worldTransform * glm::vec4(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z, 1.0f);
            v.position = glm::vec3(worldPos);

            if (mesh->HasNormals())
            {
                v.normal = normalMatrix * glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            }
            else
            {
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            // Aplicar conversión de ejes y escala global
            ApplyAxisAndScale(v.position, v.normal, settings);

            if (mesh->mTextureCoords[0])
            {
                v.texCoords = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
            }
            else
            {
                v.texCoords = glm::vec2(0.0f, 0.0f);
            }

            outVertices.push_back(v);
        }

        // Extraer índices (invirtiendo orden si la conversión de ejes o escala negativa reflejó la geometría)
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const aiFace& face = mesh->mFaces[j];
            if (flipWinding && face.mNumIndices == 3)
            {
                outIndices.push_back(vertexOffset + face.mIndices[0]);
                outIndices.push_back(vertexOffset + face.mIndices[2]);
                outIndices.push_back(vertexOffset + face.mIndices[1]);
            }
            else
            {
                for (unsigned int k = 0; k < face.mNumIndices; ++k)
                {
                    outIndices.push_back(vertexOffset + face.mIndices[k]);
                }
            }
        }
    }

    // Continuar recursivamente con todos los nodos hijos
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNodeMerged(node->mChildren[i], scene, worldTransform, ignoredNodes, settings, outVertices, outIndices);
    }
}

// Recorre recursivamente la jerarquía de nodos acumulando transformaciones y crea
// una Primitive independiente por cada instancia de malla encontrada en el árbol
void ModelLoader::ProcessNodeSubMeshes(
    const aiNode* node,
    const aiScene* scene,
    const glm::mat4& parentTransform,
    const std::unordered_set<std::string>& ignoredNodes,
    const MeshImportSettings& settings,
    const std::string& path,
    std::vector<SubMeshData>& outSubMeshes)
{
    if (!node) return;

    // Saltar nodos correspondientes a cámaras o luces ignoradas
    if (node->mName.length > 0 && ignoredNodes.count(node->mName.C_Str()) > 0)
    {
        return;
    }

    // Acumular transformación jerárquica (padre * local)
    glm::mat4 worldTransform = parentTransform * AiToGlm(node->mTransformation);
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

    // Comprobar si hay reflexión especular por escala negativa o por preset de ejes
    bool nodeMirrored = glm::determinant(glm::mat3(worldTransform)) < 0.0f;
    bool flipWinding = IsAxisMirrored(settings.axisPreset) ^ nodeMirrored;

    // Crear una sub-malla por cada aiMesh asociado a este nodo
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        unsigned int meshIdx = node->mMeshes[i];
        if (meshIdx >= scene->mNumMeshes) continue;

        const aiMesh* mesh = scene->mMeshes[meshIdx];

        // Determinar el nombre legible de la pieza (priorizando el nombre del nodo sobre el de la malla)
        std::string partName;
        if (node->mName.length > 0 && std::string(node->mName.C_Str()) != "RootNode")
        {
            partName = node->mName.C_Str();
        }
        else if (mesh->mName.length > 0)
        {
            partName = mesh->mName.C_Str();
        }
        else
        {
            partName = "Part_" + std::to_string(outSubMeshes.size() + 1);
        }

        // Assimp genera nodos intermedios con sufijos "_$AssimpFbx$_Translation", "_$AssimpFbx$_GeometricTranslation", etc.
        // para representar los pivotes de 3ds Max. Limpiamos ese sufijo para conservar el nombre limpio del objeto.
        size_t assimpTag = partName.find("_$AssimpFbx$_");
        if (assimpTag != std::string::npos)
        {
            partName = partName.substr(0, assimpTag);
        }

        int subIndex = static_cast<int>(outSubMeshes.size());
        LOG("[Assimp] Creando parte [%d] '%s' (malla %u/%u): %u vértices, %u caras",
            subIndex + 1, partName.c_str(), meshIdx + 1, scene->mNumMeshes, mesh->mNumVertices, mesh->mNumFaces);

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(mesh->mNumVertices);
        indices.reserve(mesh->mNumFaces * 3);

        // Extraer vértices transformados al espacio global del modelo
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            Vertex v;
            glm::vec4 worldPos = worldTransform * glm::vec4(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z, 1.0f);
            v.position = glm::vec3(worldPos);

            if (mesh->HasNormals())
            {
                v.normal = normalMatrix * glm::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            }
            else
            {
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            // Aplicar conversión de ejes y escala global
            ApplyAxisAndScale(v.position, v.normal, settings);

            if (mesh->mTextureCoords[0])
            {
                v.texCoords = glm::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
            }
            else
            {
                v.texCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(v);
        }

        // Extraer índices respetando el orden de caras (winding order)
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const aiFace& face = mesh->mFaces[j];
            if (flipWinding && face.mNumIndices == 3)
            {
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[2]);
                indices.push_back(face.mIndices[1]);
            }
            else
            {
                for (unsigned int k = 0; k < face.mNumIndices; ++k)
                {
                    indices.push_back(face.mIndices[k]);
                }
            }
        }

        // Inicializar la Primitive con sus metadatos de origen y ajustes de importación
        auto primitive = std::make_unique<Primitive>();
        primitive->Init(std::move(vertices), std::move(indices));
        primitive->SetSourceInfo(path, subIndex);
        primitive->SetImportSettings(settings);

        SubMeshData subMesh;
        subMesh.name = partName;
        subMesh.primitive = std::move(primitive);
        outSubMeshes.push_back(std::move(subMesh));
    }

    // Procesar recursivamente los nodos hijos
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNodeSubMeshes(node->mChildren[i], scene, worldTransform, ignoredNodes, settings, path, outSubMeshes);
    }
}

std::unique_ptr<Primitive> ModelLoader::LoadFBX(const std::string& path, const MeshImportSettings& settings)
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

    // Filtrar nodos y mallas asociados a cámaras o luces embebidas si está activado
    std::unordered_set<std::string> ignoredNodeNames = CollectIgnoredNodeNames(scene, settings);
    if (!ignoredNodeNames.empty())
    {
        LOG("[Assimp] Ignorando %zu nodo(s) de cámaras/luces en '%s'", ignoredNodeNames.size(), path.c_str());
    }

    LOG("[Assimp] Archivo '%s' procesado. Mallas encontradas: %u, Materiales: %u, Texturas embebidas: %u",
        path.c_str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumTextures);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Recorrer el grafo de escena desde el nodo raíz acumulando transformaciones
    ProcessNodeMerged(scene->mRootNode, scene, glm::mat4(1.0f), ignoredNodeNames, settings, vertices, indices);

    LOG("[Assimp] Geometría cargada con éxito: %s (%zu vértices totales, %zu índices)", path.c_str(), vertices.size(), indices.size());

    auto primitive = std::make_unique<Primitive>();
    primitive->Init(std::move(vertices), std::move(indices));
    primitive->SetSourceInfo(path, -1);
    primitive->SetImportSettings(settings);
    return primitive;
}

std::vector<SubMeshData> ModelLoader::LoadFBXMeshes(const std::string& path, const MeshImportSettings& settings)
{
    std::vector<SubMeshData> result;

    LOG("[Assimp] Iniciando carga de sub-mallas: %s (Assimp v%u.%u.%u)",
        path.c_str(), aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionRevision());

    Assimp::Importer importer;

    // Post-procesado para triangular caras, generar normales suaves y ajustar UVs
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

    // Recopilar nombres de nodos de cámaras o luces para ignorarlos si procede
    std::unordered_set<std::string> ignoredNodeNames = CollectIgnoredNodeNames(scene, settings);
    if (!ignoredNodeNames.empty())
    {
        LOG("[Assimp] Ignorando %zu nodo(s) de cámaras/luces en '%s'", ignoredNodeNames.size(), path.c_str());
    }

    result.reserve(scene->mNumMeshes);

    // Recorrer el grafo de escena desde el nodo raíz generando cada sub-malla en su posición real
    ProcessNodeSubMeshes(scene->mRootNode, scene, glm::mat4(1.0f), ignoredNodeNames, settings, path, result);

    return result;
}