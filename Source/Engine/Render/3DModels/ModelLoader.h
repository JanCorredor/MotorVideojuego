#pragma once
#include <string>
#include <memory>
#include <vector>
#include "../Primitives/GeometryGenerator.h" // Includes Primitives & Vertex

struct SubMeshData
{
    std::string name;
    std::unique_ptr<Primitive> primitive;
};

class ModelLoader 
{
public:
    // Loads a file FBX / OBJ / DAE & returns a Primitive (or an array of Primitives)
    static std::unique_ptr<Primitive> LoadFBX(const std::string& path);

    // Loads a file FBX / OBJ / DAE divide every SubMesh individually
    static std::vector<SubMeshData> LoadFBXMeshes(const std::string& path);
};