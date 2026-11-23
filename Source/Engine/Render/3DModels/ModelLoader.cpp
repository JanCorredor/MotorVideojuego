#include "ModelLoader.h"
#include "../../../Utils/Log.h"
#include "../../Scene/GameObjects/Components/Component.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/version.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stack>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <limits>

namespace
{
    std::string ToLowerStr(const std::string& input)
    {
        std::string out = input;
        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
        }
        return out;
    }

    std::string NormalizeSlashesStr(const std::string& input)
    {
        std::string out = input;
        std::replace(out.begin(), out.end(), '\\', '/');
        return out;
    }

    bool IsImageExtension(const std::string& extLower)
    {
        return extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" ||
               extLower == ".dds" || extLower == ".tga" || extLower == ".bmp";
    }

    // Detecta FBX de Blender con escala en centímetros (Scale 100 + RotX -90° con OriginalUpAxis == -1 y UnitScaleFactor == 1.0):
    // - En Blender 2.8+/2.9+/3.x/4.x (ej. InteriorTest.fbx), RotX -90° ya convierte Z-Up a Y-Up, así que solo se corrige Scale 0.01.
    // - En FBX legacy de Blender 2.7x con vértices ya Y-Up (ej. BakerHouse.fbx), RotX -90° tumba el modelo hacia -Z, así que se corrige Scale 0.01 + RotX +90°.
    glm::mat4 ComputeFbxRootCorrection(const aiScene* scene)
    {
        if (!scene || !scene->mRootNode || !scene->mMetaData) return glm::mat4(1.0f);

        int32_t originalUpAxis = 0;
        if (!scene->mMetaData->Get("OriginalUpAxis", originalUpAxis) || originalUpAxis != -1)
        {
            return glm::mat4(1.0f);
        }

        double unitScaleDouble = 0.0;
        float unitScaleFloat = 0.0f;
        double unitScale = 0.0;
        if (scene->mMetaData->Get("UnitScaleFactor", unitScaleDouble))
        {
            unitScale = unitScaleDouble;
        }
        else if (scene->mMetaData->Get("UnitScaleFactor", unitScaleFloat))
        {
            unitScale = static_cast<double>(unitScaleFloat);
        }

        if (std::fabs(unitScale - 1.0) > 1e-3)
        {
            return glm::mat4(1.0f);
        }

        struct ProbeNode
        {
            const aiNode* node;
            glm::mat4 parentMat;
        };

        auto aiToGlmLocal = [](const aiMatrix4x4& m) -> glm::mat4
        {
            glm::mat4 res(1.0f);
            res[0][0] = m.a1; res[1][0] = m.a2; res[2][0] = m.a3; res[3][0] = m.a4;
            res[0][1] = m.b1; res[1][1] = m.b2; res[2][1] = m.b3; res[3][1] = m.b4;
            res[0][2] = m.c1; res[1][2] = m.c2; res[2][2] = m.c3; res[3][2] = m.c4;
            res[0][3] = m.d1; res[1][3] = m.d2; res[2][3] = m.d3; res[3][3] = m.d4;
            return res;
        };

        bool hasBlenderCmScaleAndRot = false;
        bool hasAnyVertex = false;
        glm::vec3 minBounds(std::numeric_limits<float>::max());
        glm::vec3 maxBounds(std::numeric_limits<float>::lowest());

        std::stack<ProbeNode> stack;
        stack.push({ scene->mRootNode, glm::mat4(1.0f) });
        while (!stack.empty())
        {
            ProbeNode curr = stack.top();
            stack.pop();
            if (!curr.node) continue;

            glm::mat4 worldMat = curr.parentMat * aiToGlmLocal(curr.node->mTransformation);
            if (curr.node->mNumMeshes > 0)
            {
                if (!hasBlenderCmScaleAndRot)
                {
                    float sx = glm::length(glm::vec3(worldMat[0]));
                    float sy = glm::length(glm::vec3(worldMat[1]));
                    float sz = glm::length(glm::vec3(worldMat[2]));
                    if (sx > 50.0f && sy > 50.0f && sz > 50.0f)
                    {
                        glm::vec3 yAxis = glm::vec3(worldMat[1]) / sy;
                        if (yAxis.z < -0.95f)
                        {
                            hasBlenderCmScaleAndRot = true;
                        }
                    }
                }

                for (unsigned int m = 0; m < curr.node->mNumMeshes; ++m)
                {
                    unsigned int meshIdx = curr.node->mMeshes[m];
                    if (meshIdx >= scene->mNumMeshes) continue;
                    const aiMesh* mesh = scene->mMeshes[meshIdx];
                    if (!mesh) continue;

                    for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
                    {
                        glm::vec4 wp = worldMat * glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f);
                        glm::vec3 p(wp.x, wp.y, wp.z);
                        minBounds = glm::min(minBounds, p);
                        maxBounds = glm::max(maxBounds, p);
                        hasAnyVertex = true;
                    }
                }
            }

            for (unsigned int i = curr.node->mNumChildren; i > 0; --i)
            {
                stack.push({ curr.node->mChildren[i - 1], worldMat });
            }
        }

        if (!hasBlenderCmScaleAndRot)
        {
            return glm::mat4(1.0f);
        }

        glm::mat4 invScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));

        if (hasAnyVertex)
        {
            float extentY = maxBounds.y - minBounds.y;
            float extentZ = maxBounds.z - minBounds.z;
            bool sitsOnNegZ = (extentZ > 1e-3f && maxBounds.z <= 0.05f * extentZ && minBounds.z < -0.5f * extentZ);
            bool alreadyStandsOnY = (extentY > 1e-3f && minBounds.y >= -0.15f * extentY && maxBounds.y > 0.5f * extentY);

            if (sitsOnNegZ && !alreadyStandsOnY)
            {
                glm::mat4 invRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                LOG("[Assimp] Detectado FBX legacy de Blender (RotX -90 / Scale 100, modelo tumbado en -Z). Aplicando corrección de escala y rotación.");
                return invScale * invRot;
            }
        }

        LOG("[Assimp] Detectado FBX de Blender en centímetros (Scale 100, ya en Y-Up). Aplicando corrección automática de escala (0.01).");
        return invScale;
    }

    class MaterialTextureResolver
    {
    public:
        MaterialTextureResolver(const aiScene* sc, const std::string& modelPath, const std::string& origSourceDir)
            : scene(sc)
        {
            std::filesystem::path mp(NormalizeSlashesStr(modelPath));
            modelStem = mp.stem().string();
            if (modelStem.empty()) modelStem = "Model";
            if (mp.has_parent_path())
            {
                modelDir = NormalizeSlashesStr(mp.parent_path().string());
            }
            if (!origSourceDir.empty())
            {
                originalDir = NormalizeSlashesStr(origSourceDir);
            }
            assetsDir = NormalizeSlashesStr(SceneFile::ResolveAssetsDirectory());
            texturesDir = NormalizeSlashesStr(SceneFile::ResolveTexturesDirectory());
        }

        void ResolveMaterial(unsigned int materialIndex, glm::vec4& outColor, std::string& outTexturePath)
        {
            outColor = glm::vec4(1.0f);
            outTexturePath.clear();

            if (!scene || materialIndex >= scene->mNumMaterials) return;
            const aiMaterial* mat = scene->mMaterials[materialIndex];
            if (!mat) return;

            // 1. Extraer color base / difuso
            aiColor4D baseCol(1.0f, 1.0f, 1.0f, 1.0f);
            if (mat->Get(AI_MATKEY_BASE_COLOR, baseCol) == AI_SUCCESS ||
                mat->Get(AI_MATKEY_COLOR_DIFFUSE, baseCol) == AI_SUCCESS)
            {
                outColor = glm::vec4(baseCol.r, baseCol.g, baseCol.b, baseCol.a);
            }

            float opacity = 1.0f;
            if (mat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity >= 0.0f && opacity < 1.0f)
            {
                outColor.a = opacity;
            }

            // 2. Buscar textura difusa (DIFFUSE o BASE_COLOR)
            aiString texPath;
            bool hasTex = false;
            if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0 &&
                mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS &&
                texPath.length > 0)
            {
                hasTex = true;
            }
            else if (mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
                     mat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS &&
                     texPath.length > 0)
            {
                hasTex = true;
            }

            if (!hasTex) return;

            std::string rawPath = texPath.C_Str();
            std::unordered_map<std::string, std::string>::const_iterator cachedIt = resolvedCache.find(rawPath);
            if (cachedIt != resolvedCache.end())
            {
                outTexturePath = cachedIt->second;
                return;
            }

            std::string resolved = ResolveTexturePathInternal(rawPath);
            resolvedCache[rawPath] = resolved;
            outTexturePath = resolved;
        }

    private:
        void IndexDirectory(const std::string& dirPath, int maxEntries)
        {
            if (dirPath.empty()) return;
            std::error_code ec;
            if (!std::filesystem::exists(dirPath, ec) || !std::filesystem::is_directory(dirPath, ec))
            {
                return;
            }

            int visited = 0;
            std::filesystem::recursive_directory_iterator it(
                dirPath,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            std::filesystem::recursive_directory_iterator end;

            while (!ec && it != end && visited < maxEntries)
            {
                ++visited;
                std::error_code fileEc;
                if (it->is_regular_file(fileEc))
                {
                    std::filesystem::path p = it->path();
                    std::string extLower = ToLowerStr(p.extension().string());
                    if (IsImageExtension(extLower))
                    {
                        std::string fnameLower = ToLowerStr(p.filename().string());
                        if (filenameIndex.find(fnameLower) == filenameIndex.end())
                        {
                            filenameIndex[fnameLower] = NormalizeSlashesStr(p.string());
                        }
                    }
                }
                it.increment(ec);
            }
        }

        void EnsureFilenameIndexBuilt()
        {
            if (indexBuilt) return;
            indexBuilt = true;

            // Prioridad 1: carpetas Texture / Textures junto al FBX original (ej. Downloads/InteriorTest/Texture)
            if (!originalDir.empty())
            {
                IndexDirectory(originalDir + "/Texture", 2000);
                IndexDirectory(originalDir + "/Textures", 2000);
                IndexDirectory(originalDir + "/texture", 2000);
                IndexDirectory(originalDir + "/textures", 2000);
                IndexDirectory(originalDir, 500);
            }

            // Prioridad 2: carpeta donde está el modelo en el proyecto
            if (!modelDir.empty() && modelDir != originalDir)
            {
                IndexDirectory(modelDir + "/Texture", 2000);
                IndexDirectory(modelDir + "/Textures", 2000);
                IndexDirectory(modelDir, 500);
            }

            // Prioridad 3: carpeta de texturas y assets del proyecto
            IndexDirectory(texturesDir, 2000);
            IndexDirectory(assetsDir, 1000);
        }

        std::string ExtractEmbeddedTexture(const aiTexture* embeddedTex, const std::string& rawKey)
        {
            if (!embeddedTex || embeddedTex->mHeight != 0 || embeddedTex->mWidth == 0 || !embeddedTex->pcData)
            {
                return "";
            }

            std::string ext = "png";
            if (embeddedTex->achFormatHint[0] != '\0')
            {
                ext = ToLowerStr(std::string(embeddedTex->achFormatHint));
                if (ext == "jpeg") ext = "jpg";
            }

            std::string safeTag = rawKey;
            for (size_t i = 0; i < safeTag.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(safeTag[i]);
                if (!std::isalnum(c)) safeTag[i] = '_';
            }

            std::string fileName = modelStem + "_embedded" + safeTag + "." + ext;
            std::filesystem::path outPath = std::filesystem::path(texturesDir) / fileName;
            std::error_code ec;
            std::filesystem::create_directories(texturesDir, ec);

            bool shouldWrite = true;
            if (std::filesystem::exists(outPath, ec))
            {
                uintmax_t existingSize = std::filesystem::file_size(outPath, ec);
                if (!ec && existingSize == static_cast<uintmax_t>(embeddedTex->mWidth))
                {
                    shouldWrite = false;
                }
            }

            if (shouldWrite)
            {
                std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
                if (!out.is_open())
                {
                    LOG_WARNING("[Assimp] No se pudo extraer textura embebida a '%s'", outPath.string().c_str());
                    return "";
                }
                out.write(reinterpret_cast<const char*>(embeddedTex->pcData), static_cast<std::streamsize>(embeddedTex->mWidth));
                out.close();
            }

            return SceneFile::ToProjectRelativePath(NormalizeSlashesStr(outPath.string()));
        }

        std::string ResolveTexturePathInternal(const std::string& rawTexPath)
        {
            if (rawTexPath.empty()) return "";

            // 1. Comprobar textura embebida (*0, *1 o por nombre)
            if (scene)
            {
                const aiTexture* embedded = scene->GetEmbeddedTexture(rawTexPath.c_str());
                if (embedded)
                {
                    return ExtractEmbeddedTexture(embedded, rawTexPath);
                }
            }

            std::string normRaw = NormalizeSlashesStr(rawTexPath);
            std::filesystem::path rawP(normRaw);
            std::string filename = rawP.filename().string();

            std::error_code ec;

            // 2. Comprobar rutas candidatas directas
            std::vector<std::filesystem::path> candidates;
            if (rawP.is_absolute())
            {
                candidates.push_back(rawP);
            }
            if (!originalDir.empty())
            {
                if (!rawP.is_absolute()) candidates.push_back(std::filesystem::path(originalDir) / rawP);
                if (!filename.empty()) candidates.push_back(std::filesystem::path(originalDir) / filename);
            }
            if (!modelDir.empty())
            {
                if (!rawP.is_absolute()) candidates.push_back(std::filesystem::path(modelDir) / rawP);
                if (!filename.empty()) candidates.push_back(std::filesystem::path(modelDir) / filename);
            }
            if (!texturesDir.empty())
            {
                if (!rawP.is_absolute()) candidates.push_back(std::filesystem::path(texturesDir) / rawP);
                if (!filename.empty()) candidates.push_back(std::filesystem::path(texturesDir) / filename);
            }
            if (!assetsDir.empty())
            {
                if (!rawP.is_absolute()) candidates.push_back(std::filesystem::path(assetsDir) / rawP);
                if (!filename.empty()) candidates.push_back(std::filesystem::path(assetsDir) / filename);
            }

            for (size_t i = 0; i < candidates.size(); ++i)
            {
                if (std::filesystem::exists(candidates[i], ec) && std::filesystem::is_regular_file(candidates[i], ec))
                {
                    return NormalizeSlashesStr(candidates[i].string());
                }
            }

            // 3. Búsqueda por nombre de archivo en carpetas Texture/Textures/Assets (indexado una sola vez)
            if (!filename.empty())
            {
                EnsureFilenameIndexBuilt();
                std::string fnameLower = ToLowerStr(filename);
                std::unordered_map<std::string, std::string>::const_iterator found = filenameIndex.find(fnameLower);
                if (found != filenameIndex.end())
                {
                    return found->second;
                }
            }

            return "";
        }

        const aiScene* scene = nullptr;
        std::string modelStem;
        std::string modelDir;
        std::string originalDir;
        std::string assetsDir;
        std::string texturesDir;
        bool indexBuilt = false;
        std::unordered_map<std::string, std::string> filenameIndex;
        std::unordered_map<std::string, std::string> resolvedCache;
    };
}

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
    const aiNode* rootNode,
    const aiScene* scene,
    const glm::mat4& rootParentTransform,
    const std::unordered_set<std::string>& ignoredNodes,
    const MeshImportSettings& settings,
    std::vector<Vertex>& outVertices,
    std::vector<unsigned int>& outIndices)
{
    if (!rootNode) return;

    struct NodeTask
    {
        const aiNode* node;
        glm::mat4 parentTransform;
    };

    std::stack<NodeTask> stack;
    stack.push({ rootNode, rootParentTransform });

    while (!stack.empty())
    {
        NodeTask current = stack.top();
        stack.pop();

        const aiNode* node = current.node;
        if (!node) continue;

        // Si el nodo pertenece a una cámara o luz ignorada, descartamos esta rama
        if (node->mName.length > 0 && ignoredNodes.count(node->mName.C_Str()) > 0)
        {
            continue;
        }

        // Calcular matriz acumulada del nodo en espacio de escena (incluye pivotes y offsets de 3ds Max)
        glm::mat4 worldTransform = current.parentTransform * AiToGlm(node->mTransformation);

        // Matriz normal (inversa transpuesta de la parte 3x3) para transformar normales correctamente
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

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

        // Apilar hijos en orden inverso para preservar exactamente el orden de recorrido pre-orden
        for (unsigned int i = node->mNumChildren; i > 0; --i)
        {
            stack.push({ node->mChildren[i - 1], worldTransform });
        }
    }
}

// Recorre iterativamente con std::stack la jerarquía de nodos acumulando transformaciones y crea
// una Primitive independiente por cada instancia de malla encontrada en el árbol
void ModelLoader::ProcessNodeSubMeshes(
    const aiNode* rootNode,
    const aiScene* scene,
    const glm::mat4& rootParentTransform,
    const std::unordered_set<std::string>& ignoredNodes,
    const MeshImportSettings& settings,
    const std::string& path,
    const std::string& originalSourceDir,
    std::vector<SubMeshData>& outSubMeshes)
{
    if (!rootNode) return;

    MaterialTextureResolver materialResolver(scene, path, originalSourceDir);

    struct NodeTask
    {
        const aiNode* node;
        glm::mat4 parentTransform;
    };

    std::stack<NodeTask> stack;
    stack.push({ rootNode, rootParentTransform });

    while (!stack.empty())
    {
        NodeTask current = stack.top();
        stack.pop();

        const aiNode* node = current.node;
        if (!node) continue;

        // Saltar nodos correspondientes a cámaras o luces ignoradas
        if (node->mName.length > 0 && ignoredNodes.count(node->mName.C_Str()) > 0)
        {
            continue;
        }

        glm::mat4 worldTransform = current.parentTransform * AiToGlm(node->mTransformation);
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(worldTransform)));

        bool nodeMirrored = glm::determinant(glm::mat3(worldTransform)) < 0.0f;
        bool flipWinding = IsAxisMirrored(settings.axisPreset) ^ nodeMirrored;

        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            unsigned int meshIdx = node->mMeshes[i];
            if (meshIdx >= scene->mNumMeshes) continue;

            const aiMesh* mesh = scene->mMeshes[meshIdx];

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

            auto primitive = std::make_unique<Primitive>();
            primitive->Init(std::move(vertices), std::move(indices));
            primitive->SetSourceInfo(path, subIndex);
            primitive->SetImportSettings(settings);

            SubMeshData subMesh;
            subMesh.name = partName;
            subMesh.primitive = std::move(primitive);
            materialResolver.ResolveMaterial(mesh->mMaterialIndex, subMesh.diffuseColor, subMesh.diffuseTexturePath);
            outSubMeshes.push_back(std::move(subMesh));
        }

        for (unsigned int i = node->mNumChildren; i > 0; --i)
        {
            stack.push({ node->mChildren[i - 1], worldTransform });
        }
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

    glm::mat4 rootCorrection = ComputeFbxRootCorrection(scene);

    // Recorrer el grafo de escena con std::stack desde el nodo raíz acumulando transformaciones
    ProcessNodeMerged(scene->mRootNode, scene, rootCorrection, ignoredNodeNames, settings, vertices, indices);

    LOG("[Assimp] Geometría cargada con éxito: %s (%zu vértices totales, %zu índices)", path.c_str(), vertices.size(), indices.size());

    auto primitive = std::make_unique<Primitive>();
    primitive->Init(std::move(vertices), std::move(indices));
    primitive->SetSourceInfo(path, -1);
    primitive->SetImportSettings(settings);
    return primitive;
}

std::vector<SubMeshData> ModelLoader::LoadFBXMeshes(const std::string& path, const MeshImportSettings& settings, const std::string& originalSourceDir)
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

    LOG("[Assimp] Archivo '%s' procesado. Partes/Mallas encontradas: %u, Materiales: %u, Texturas embebidas: %u",
        path.c_str(), scene->mNumMeshes, scene->mNumMaterials, scene->mNumTextures);

    // Recopilar nombres de nodos de cámaras o luces para ignorarlos si procede
    std::unordered_set<std::string> ignoredNodeNames = CollectIgnoredNodeNames(scene, settings);
    if (!ignoredNodeNames.empty())
    {
        LOG("[Assimp] Ignorando %zu nodo(s) de cámaras/luces en '%s'", ignoredNodeNames.size(), path.c_str());
    }

    result.reserve(scene->mNumMeshes);
    glm::mat4 rootCorrection = ComputeFbxRootCorrection(scene);

    // Recorrer el grafo de escena con std::stack desde el nodo raíz generando cada sub-malla en su posición real
    ProcessNodeSubMeshes(scene->mRootNode, scene, rootCorrection, ignoredNodeNames, settings, path, originalSourceDir, result);

    return result;
}