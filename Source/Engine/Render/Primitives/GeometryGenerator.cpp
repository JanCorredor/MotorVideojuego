#include "GeometryGenerator.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

// -------------------------------------------------------------
// 1. Cube
// -------------------------------------------------------------
std::unique_ptr<Primitive> GeometryGenerator::CreateCube(float size) 
{
    float h = size * 0.5f;

    std::vector<Vertex> vertices = {
        // Front face
        {{-h, -h,  h}, {0, 0, 1}, {0, 0}}, {{ h, -h,  h}, {0, 0, 1}, {1, 0}},
        {{ h,  h,  h}, {0, 0, 1}, {1, 1}}, {{-h,  h,  h}, {0, 0, 1}, {0, 1}},
        // Back face
        {{-h, -h, -h}, {0, 0,-1}, {1, 0}}, {{-h,  h, -h}, {0, 0,-1}, {1, 1}},
        {{ h,  h, -h}, {0, 0,-1}, {0, 1}}, {{ h, -h, -h}, {0, 0,-1}, {0, 0}},
        // Top face
        {{-h,  h, -h}, {0, 1, 0}, {0, 1}}, {{-h,  h,  h}, {0, 1, 0}, {0, 0}},
        {{ h,  h,  h}, {0, 1, 0}, {1, 0}}, {{ h,  h, -h}, {0, 1, 0}, {1, 1}},
        // Bottom face
        {{-h, -h, -h}, {0,-1, 0}, {0, 0}}, {{ h, -h, -h}, {0,-1, 0}, {1, 0}},
        {{ h, -h,  h}, {0,-1, 0}, {1, 1}}, {{-h, -h,  h}, {0,-1, 0}, {0, 1}},
        // Right face
        {{ h, -h, -h}, {1, 0, 0}, {1, 0}}, {{ h,  h, -h}, {1, 0, 0}, {1, 1}},
        {{ h,  h,  h}, {1, 0, 0}, {0, 1}}, {{ h, -h,  h}, {1, 0, 0}, {0, 0}},
        // Left face
        {{-h, -h, -h}, {-1,0, 0}, {0, 0}}, {{-h, -h,  h}, {-1,0, 0}, {1, 0}},
        {{-h,  h,  h}, {-1,0, 0}, {1, 1}}, {{-h,  h, -h}, {-1,0, 0}, {0, 1}}
    };

    std::vector<unsigned int> indices;
    for (int i = 0; i < 6; ++i) {
        unsigned int offset = i * 4;
        indices.push_back(offset + 0); indices.push_back(offset + 1); indices.push_back(offset + 2);
        indices.push_back(offset + 0); indices.push_back(offset + 2); indices.push_back(offset + 3);
    }

    auto cube = std::make_unique<Primitive>();
    cube->Init(vertices, indices);
    return cube;
}

// -------------------------------------------------------------
// 2. Pyramid
// -------------------------------------------------------------
std::unique_ptr<Primitive> GeometryGenerator::CreatePyramid(float baseSize, float height) 
{
    float hBase = baseSize * 0.5f;
    float hApex = height * 0.5f;

    // Calculamos la inclinación de la normal para las caras laterales
    glm::vec3 apex(0.0f, hApex, 0.0f);

    // Normales de cada cara lateral
    glm::vec3 nFront = glm::normalize(glm::cross(glm::vec3(hBase, -hApex, hBase) - glm::vec3(-hBase, -hApex, hBase), apex - glm::vec3(-hBase, -hApex, hBase)));
    glm::vec3 nRight = glm::normalize(glm::cross(glm::vec3(hBase, -hApex, -hBase) - glm::vec3(hBase, -hApex, hBase), apex - glm::vec3(hBase, -hApex, hBase)));
    glm::vec3 nBack = glm::normalize(glm::cross(glm::vec3(-hBase, -hApex, -hBase) - glm::vec3(hBase, -hApex, -hBase), apex - glm::vec3(hBase, -hApex, -hBase)));
    glm::vec3 nLeft = glm::normalize(glm::cross(glm::vec3(-hBase, -hApex, hBase) - glm::vec3(-hBase, -hApex, -hBase), apex - glm::vec3(-hBase, -hApex, -hBase)));

    std::vector<Vertex> vertices = {
        // --- Cara Frontal ---
        {{-hBase, -hApex,  hBase}, nFront, {0.0f, 0.0f}},
        {{ hBase, -hApex,  hBase}, nFront, {1.0f, 0.0f}},
        {{ 0.0f,   hApex,   0.0f}, nFront, {0.5f, 1.0f}},

        // --- Cara Derecha ---
        {{ hBase, -hApex,  hBase}, nRight, {0.0f, 0.0f}},
        {{ hBase, -hApex, -hBase}, nRight, {1.0f, 0.0f}},
        {{ 0.0f,   hApex,   0.0f}, nRight, {0.5f, 1.0f}},

        // --- Cara Trasera ---
        {{ hBase, -hApex, -hBase}, nBack,  {0.0f, 0.0f}},
        {{-hBase, -hApex, -hBase}, nBack,  {1.0f, 0.0f}},
        {{ 0.0f,   hApex,   0.0f}, nBack,  {0.5f, 1.0f}},

        // --- Cara Izquierda ---
        {{-hBase, -hApex, -hBase}, nLeft,  {0.0f, 0.0f}},
        {{-hBase, -hApex,  hBase}, nLeft,  {1.0f, 0.0f}},
        {{ 0.0f,   hApex,   0.0f}, nLeft,  {0.5f, 1.0f}},

        // --- Base ---
        {{-hBase, -hApex, -hBase}, {0, -1, 0}, {0.0f, 0.0f}},
        {{ hBase, -hApex, -hBase}, {0, -1, 0}, {1.0f, 0.0f}},
        {{ hBase, -hApex,  hBase}, {0, -1, 0}, {1.0f, 1.0f}},
        {{-hBase, -hApex,  hBase}, {0, -1, 0}, {0.0f, 1.0f}}
    };

    std::vector<unsigned int> indices = 
    {
        // Caras laterales
        0, 1, 2,
        3, 4, 5,
        6, 7, 8,
        9, 10, 11,
        // Base
        12, 14, 13,
        12, 15, 14
    };

    auto pyramid = std::make_unique<Primitive>();
    pyramid->Init(vertices, indices);
    return pyramid;
}

// -------------------------------------------------------------
// 3. Sphere
// -------------------------------------------------------------
std::unique_ptr<Primitive> GeometryGenerator::CreateSphere(float radius, unsigned int rings, unsigned int sectors) 
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float const R = 1.0f / (float)(rings);
    float const S = 1.0f / (float)(sectors);

    for (unsigned int r = 0; r <= rings; ++r) {
        float const phi = -glm::half_pi<float>() + glm::pi<float>() * r * R; // -PI/2 to PI/2
        float const y = std::sin(phi);
        float const ringRadius = std::cos(phi); 

        for (unsigned int s = 0; s <= sectors; ++s) {
            float const theta = 2.0f * glm::pi<float>() * s * S; // 0 to 2*PI
            float const x = ringRadius * std::cos(theta);
            float const z = ringRadius * std::sin(theta);

            Vertex v;
            v.position = glm::vec3(x, y, z) * radius;
            v.normal = glm::vec3(x, y, z); // Outward pointing normal
            v.texCoords = glm::vec2(s * S, r * R);
            vertices.push_back(v);
        }
    }

    // Generate Indices with Counter-Clockwise (CCW) winding
    unsigned int const stride = sectors + 1;
    for (unsigned int r = 0; r < rings; ++r) {
        for (unsigned int s = 0; s < sectors; ++s) {
            unsigned int current = r * stride + s;
            unsigned int next = r * stride + (s + 1);
            unsigned int bottom = (r + 1) * stride + s;
            unsigned int bottomNext = (r + 1) * stride + (s + 1);

            // First triangle (CCW)
            indices.push_back(current);
            indices.push_back(bottomNext);
            indices.push_back(next);

            // Second triangle (CCW)
            indices.push_back(current);
            indices.push_back(bottom);
            indices.push_back(bottomNext);
        }
    }

    auto sphere = std::make_unique<Primitive>();
    sphere->Init(vertices, indices);
    return sphere;
}

// -------------------------------------------------------------
// 4. Cylinder
// -------------------------------------------------------------
std::unique_ptr<Primitive> GeometryGenerator::CreateCylinder(float radius, float height, unsigned int sectors) 
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float h2 = height * 0.5f;
    float sectorStep = 2.0f * glm::pi<float>() / sectors;

    // Side vertices
    for (unsigned int i = 0; i <= sectors; ++i) {
        float sectorAngle = i * sectorStep;
        float x = cos(sectorAngle);
        float z = sin(sectorAngle);

        // Bottom vertex
        vertices.push_back({{x * radius, -h2, z * radius}, {x, 0.0f, z}, {(float)i / sectors, 0.0f}});

        // Top vertex
        vertices.push_back({{x * radius,  h2, z * radius}, {x, 0.0f, z}, {(float)i / sectors, 1.0f}});
    }

    // Side indices
    for (unsigned int i = 0; i < sectors; ++i) {
        unsigned int k1 = i * 2;
        unsigned int k2 = k1 + 1;
        unsigned int k3 = (i + 1) * 2;
        unsigned int k4 = k3 + 1;

        indices.push_back(k1); indices.push_back(k3); indices.push_back(k2);
        indices.push_back(k2); indices.push_back(k3); indices.push_back(k4);
    }

    auto cylinder = std::make_unique<Primitive>();
    cylinder->Init(vertices, indices);
    return cylinder;
}