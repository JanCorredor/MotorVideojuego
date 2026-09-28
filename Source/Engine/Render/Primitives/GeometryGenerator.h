#pragma once
#include "Primitive.h"
#include <memory>

class GeometryGenerator 
{
    public:
    static std::unique_ptr<Primitive> CreateCube(float size = 1.0f);
    static std::unique_ptr<Primitive> CreatePyramid(float baseSize = 1.0f, float height = 1.0f);
    static std::unique_ptr<Primitive> CreateSphere(float radius = 0.5f, unsigned int rings = 16, unsigned int sectors = 32);
    static std::unique_ptr<Primitive> CreateCylinder(float radius = 0.5f, float height = 1.0f, unsigned int sectors = 32);
};