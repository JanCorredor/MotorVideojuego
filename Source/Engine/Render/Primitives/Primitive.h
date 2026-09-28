#pragma once
#include <vector>
#include <glad/glad.h>
#include "Vertex.h"

class Primitive 
{
public:
    Primitive() = default;
    ~Primitive();

    void Init(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void Draw() const;

private:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int indexCount = 0;
};