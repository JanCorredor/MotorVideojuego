#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "Vertex.h"

// Datos geométricos de un triángulo (calculados una vez en Init) para visualizar normales por cara
struct FaceData
{
    glm::vec3 centroid;
    glm::vec3 normal;
};

class Primitive
{
public:
    Primitive() = default;
    ~Primitive();

    // Los IDs de OpenGL no son copiables
    Primitive(const Primitive&) = delete;
    Primitive& operator=(const Primitive&) = delete;

    void Init(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices);
    void Draw() const;

    const std::vector<Vertex>& GetVertices() const { return vertices; }
    const std::vector<unsigned int>& GetIndices() const { return indices; }
    const std::vector<FaceData>& GetFaces() const { return faces; }

    size_t GetVertexCount() const { return vertices.size(); }
    size_t GetTriangleCount() const { return indices.size() / 3; }

    glm::vec3 GetBoundsMin() const { return boundsMin; }
    glm::vec3 GetBoundsMax() const { return boundsMax; }

private:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int indexCount = 0;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<FaceData> faces;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
};