#pragma once
#include <string>
#include <memory>
#include <vector>
#include "../Primitives/GeometryGenerator.h" // Includes Primitives & Vertex

class ModelLoader 
{
public:
    // Loads a file FBX / OBJ / DAE & returns a Primitive (or an array of Primitives)
    static std::unique_ptr<Primitive> LoadFBX(const std::string& path);
};