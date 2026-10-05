#pragma once
#include "Component.h"
#include "TransformComponent.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class GameObject;

class CameraComponent : public Component
{
public:
    CameraComponent(GameObject* owner);
    ~CameraComponent() override = default;

    void Update(float dt) override;

    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix(float aspectRatio);

    void FocusOnObject(GameObject* target);

    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

private:
    void UpdateCameraVectors();

    TransformComponent* transform = nullptr;
    GameObject* selectedObject = nullptr;

    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;

    float movementSpeed = 5.0f;
    float mouseSensitivity = 0.1f;
    float orbitDistance = 5.0f;
};