#pragma once
#include "Primitive.h"
#include <memory>

enum class PrimitiveType
{
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Pyramid
};

class GeometryGenerator 
{
    public:
    static std::unique_ptr<Primitive> CreateCube(float size = 1.0f);
    static std::unique_ptr<Primitive> CreatePyramid(float baseSize = 1.0f, float height = 1.0f);
    static std::unique_ptr<Primitive> CreateSphere(float radius = 0.5f, unsigned int rings = 16, unsigned int sectors = 32);
    static std::unique_ptr<Primitive> CreateCylinder(float radius = 0.5f, float height = 1.0f, unsigned int sectors = 32);
    static std::unique_ptr<Primitive> CreatePlane(float size = 1.0f);

    // Crea una primitiva básica con sus parámetros por defecto
    static std::unique_ptr<Primitive> Create(PrimitiveType type);
    static const char* GetName(PrimitiveType type);
};