#pragma once
#include <vector>
#include "Vertex.h"

class Primitive
{
public:
    Primitive() = default;
    ~Primitive();

    void Init(const std::vector<Vertex>& verts, const std::vector<unsigned int>& indices);
    void Draw() const;

    const std::vector<Vertex>& GetVertices() const { return vertices; }

private:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int indexCount = 0;
    std::vector<Vertex> vertices;
};