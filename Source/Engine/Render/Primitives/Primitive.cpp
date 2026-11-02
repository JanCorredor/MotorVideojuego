#include "Primitive.h"
#include <glad/glad.h>


Primitive::~Primitive() 
{
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    if (EBO != 0) glDeleteBuffers(1, &EBO);
}

void Primitive::Init(std::vector<Vertex>&& verts, std::vector<unsigned int>&& inds)
{
    // Movemos los datos en lugar de copiarlos
    this->vertices = std::move(verts);
    this->indices = std::move(inds);

    indexCount = static_cast<unsigned int>(indices.size());

    // Bounds locales
    if (!vertices.empty())
    {
        boundsMin = boundsMax = vertices[0].position;
        for (const Vertex& v : vertices)
        {
            boundsMin = glm::min(boundsMin, v.position);
            boundsMax = glm::max(boundsMax, v.position);
        }
    }

    // Normales por cara
    faces.clear();
    faces.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const glm::vec3& a = vertices[indices[i]].position;
        const glm::vec3& b = vertices[indices[i + 1]].position;
        const glm::vec3& c = vertices[indices[i + 2]].position;
        glm::vec3 n = glm::cross(b - a, c - a);
        float len = glm::length(n);
        FaceData face;
        face.centroid = (a + b + c) / 3.0f;
        face.normal = (len > 1e-8f) ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
        faces.push_back(face);
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Attribute 0: Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // Attribute 1: Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // Attribute 2: TexCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    glBindVertexArray(0);

    // Ajustar capacidades de los vectores para liberar memoria no utilizada
    vertices.shrink_to_fit();
    indices.shrink_to_fit();
    faces.shrink_to_fit();
}

void Primitive::Draw() const 
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

Primitive::Primitive(Primitive&& other) noexcept
{
    VAO = other.VAO;
    VBO = other.VBO;
    EBO = other.EBO;
    vertices = std::move(other.vertices);
    indices = std::move(other.indices);
    faces = std::move(other.faces);
    boundsMin = other.boundsMin;
    boundsMax = other.boundsMax;

    // Anulamos los IDs del objeto origen para que su destructor no los borre de la GPU
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
}

Primitive& Primitive::operator=(Primitive&& other) noexcept
{
    if (this != &other)
    {
        // Liberar buffers actuales
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (EBO != 0) glDeleteBuffers(1, &EBO);
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);

        // Robar los recursos del otro objeto
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
        faces = std::move(other.faces);
        boundsMin = other.boundsMin;
        boundsMax = other.boundsMax;

        // Resetear el origen
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
    }
    return *this;
}
std::unique_ptr<Primitive> Primitive::Clone() const
{
    auto clone = std::make_unique<Primitive>();
    std::vector<Vertex> vCopy = vertices;
    std::vector<unsigned int> iCopy = indices;
    clone->Init(std::move(vCopy), std::move(iCopy));
    return clone;
}
