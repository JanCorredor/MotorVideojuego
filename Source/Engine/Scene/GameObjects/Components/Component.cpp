#include "Component.h"

const char* ComponentTypeToString(ComponentType type)
{
	switch (type)
	{
	case COMPONENT_TRANSFORM: return "Transform";
	case COMPONENT_MESH:      return "Mesh";
	case COMPONENT_MATERIAL:  return "Material";
	case COMPONENT_CAMERA:    return "Camera";
	default:                  return "Unknown";
	}
}

bool ComponentTypeFromString(const std::string& name, ComponentType& outType)
{
	for (int i = 0; i < COMPONENT_COUNT; ++i)
	{
		if (name == ComponentTypeToString(static_cast<ComponentType>(i)))
		{
			outType = static_cast<ComponentType>(i);
			return true;
		}
	}
	return false;
}
