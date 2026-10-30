#include "CameraComponent.h"
#include "../../../Engine.h"
#include "../../../Input/Input.h"
#include "../../../Window/Window.h"
#include "../GameObject.h"
#include "../../Scene.h"
#include "MeshComponent.h"
#include "../../../../Utils/Log.h"
#include <algorithm>

CameraComponent::CameraComponent(GameObject* owner) : Component(COMPONENT_CAMERA, owner)
{
    transform = owner->GetComponent<TransformComponent>();
    UpdateCameraVectors();
}

void CameraComponent::Update(float dt)
{
    if (!transform) return;

    Input* input = Engine::GetInstance().input.get();
    Scene* scene = Engine::GetInstance().scene.get();
    GameObject* selectedObject = scene ? scene->GetSelected() : nullptr;

    SDL_Window* sdlWindow = Engine::GetInstance().window->GetWindow();
    bool windowHasFocus = !input->GetWindowEvent(WE_HIDE);

    if (!windowHasFocus)
    {
        if (wasRelativeMouseMode)
        {
            SDL_SetWindowRelativeMouseMode(sdlWindow, false);
            wasRelativeMouseMode = false;
        }
        isControllingCamera = false;
        return; // Salimos en este frame si no hay foco
    }

    bool rmbDown = (input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_DOWN);
    bool rmbHeld = (rmbDown || input->GetMouseButton(SDL_BUTTON_RIGHT) == KEY_REPEAT);
    bool altHeld = (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT);
    bool lmbDown = (input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_DOWN);
    bool lmbHeld = (lmbDown || input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_REPEAT);

    // Solo iniciamos el control de cámara si el clic comienza dentro del viewport de Scene
    if ((rmbDown || (altHeld && lmbDown && selectedObject != nullptr)) && viewportHovered && !isControllingCamera)
    {
        isControllingCamera = true;
    }


    if (!rmbHeld && !(altHeld && lmbHeld))
    {
        isControllingCamera = false;
    }

    bool isFreeLooking = isControllingCamera && rmbHeld;
    bool isOrbiting = isControllingCamera && altHeld && lmbHeld && selectedObject != nullptr;
    bool activeControl = isFreeLooking || isOrbiting;
    isControllingCamera = activeControl;

    // Comprobar si la ventana actual sigue teniendo el foco
    if (activeControl && !wasRelativeMouseMode)
    {
        SDL_GetMouseState(&lockedMouseX, &lockedMouseY);
        SDL_CaptureMouse(false);
        SDL_SetWindowRelativeMouseMode(sdlWindow, true);
        wasRelativeMouseMode = true;
    }
    // Se fuerza la liberación si se pierde el control O la ventana pierde el foco
    else if ((!activeControl) && wasRelativeMouseMode)
    {
        SDL_SetWindowRelativeMouseMode(sdlWindow, false);
        SDL_WarpMouseInWindow(sdlWindow, lockedMouseX, lockedMouseY);
        wasRelativeMouseMode = false;
        isControllingCamera = false;
    }

    float currentSpeed = movementSpeed;
    if (input->GetKey(SDL_SCANCODE_LSHIFT))
    {
        currentSpeed *= 2.0f;
    }

    // Rueda del ratón: Zoom (solo cuando el cursor está sobre el viewport de Scene o controlando la cámara)
    if (viewportHovered || activeControl)
    {
        float scroll = input->GetMouseScrollDelta();
        if (scroll != 0.0f)
        {
            fov -= scroll * 2.0f;
            fov = std::max(1.0f, std::min(fov, 120.0f));
        }
    }

    // Botón derecho: Free look y WASD
    if (isFreeLooking)
    {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        yaw += mouseDelta.x * mouseSensitivity;
        pitch -= mouseDelta.y * mouseSensitivity;
        pitch = std::max(-89.0f, std::min(pitch, 89.0f));

        UpdateCameraVectors();

        glm::vec3 pos = transform->GetPosition();
        float step = currentSpeed * (dt / 1000.0f);

        if (input->GetKey(SDL_SCANCODE_W)) pos += front * step;
        if (input->GetKey(SDL_SCANCODE_S)) pos -= front * step;
        if (input->GetKey(SDL_SCANCODE_A)) pos -= right * step;
        if (input->GetKey(SDL_SCANCODE_D)) pos += right * step;
        if (input->GetKey(SDL_SCANCODE_E)) pos += worldUp * step;
        if (input->GetKey(SDL_SCANCODE_Q)) pos -= worldUp * step;

        transform->SetPosition(pos);
    }

    // Alt + Click Izquierdo: Orbitar objeto seleccionado
    if (isOrbiting && selectedObject)
    {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        yaw += mouseDelta.x * mouseSensitivity;
        pitch -= mouseDelta.y * mouseSensitivity;
        pitch = std::max(-89.0f, std::min(pitch, 89.0f));

        UpdateCameraVectors();

        glm::vec3 targetPos = glm::vec3(selectedObject->GetWorldMatrix()[3]);
        glm::vec3 newPos = targetPos - (front * orbitDistance);
        transform->SetPosition(newPos);
    }

    // Tecla F: Focus en el objeto seleccionado (cuando el viewport está activo)
    if (viewportHovered && input->GetKeyDown(SDL_SCANCODE_F) && selectedObject)
    {
        FocusOnObject(selectedObject);
    }
}

void CameraComponent::UpdateCameraVectors()
{
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    front = glm::normalize(newFront);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

glm::mat4 CameraComponent::GetViewMatrix()
{
    glm::vec3 pos = transform ? transform->GetPosition() : glm::vec3(0.0f);
    return glm::lookAt(pos, pos + front, up);
}

glm::mat4 CameraComponent::GetProjectionMatrix(float aspectRatio)
{
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

void CameraComponent::FocusOnObject(GameObject* target)
{
    if (!target || !transform) return;
    if (Engine::GetInstance().scene)
    {
        Engine::GetInstance().scene->SetSelected(target);
    }
    glm::vec3 targetPos = glm::vec3(target->GetWorldMatrix()[3]);
    transform->SetPosition(targetPos - (front * orbitDistance));
}

void CameraComponent::ResetCamera()
{
    yaw = -90.0f;
    pitch = 0.0f;
    fov = 45.0f;
    UpdateCameraVectors();
    if (transform)
    {
        transform->SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
    }
}

bool CameraComponent::RayAABBIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& boxMin, const glm::vec3& boxMax, float& tMinOut)
{
    glm::vec3 invDir = 1.0f / rayDir;
    glm::vec3 t0s = (boxMin - rayOrigin) * invDir;
    glm::vec3 t1s = (boxMax - rayOrigin) * invDir;

    glm::vec3 tsmaller = glm::min(t0s, t1s);
    glm::vec3 tbigger = glm::max(t0s, t1s);

    float tmin = std::max({ tsmaller.x, tsmaller.y, tsmaller.z });
    float tmax = std::min({ tbigger.x, tbigger.y, tbigger.z });

    if (tmax >= tmin && tmax >= 0.0f)
    {
        tMinOut = (tmin >= 0.0f) ? tmin : tmax;
        return true;
    }
    return false;
}

bool CameraComponent::RayTriangleIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& tOut)
{
    const float EPSILON = 1e-6f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDir, edge2);
    float a = glm::dot(edge1, h);

    // Si 'a' es cercano a 0, el rayo es paralelo al triángulo
    if (a > -EPSILON && a < EPSILON) return false;

    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(rayDir, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    // Calculamos 't' para saber a qué distancia en el rayo está el punto de impacto
    float t = f * glm::dot(edge2, q);
    if (t > EPSILON)
    {
        tOut = t;
        return true;
    }
    return false;
}

// Recorrido recursivo: Transforma el rayo a Local Space -> Prueba AABB -> Prueba Triángulos
void CameraComponent::CheckRaycastRecursive(GameObject* go, glm::vec3 rayOrigin, glm::vec3 rayDir, GameObject*& closestObject, float& minDistance, const glm::mat4& parentMatrix)
{
    if (!go->IsActive()) return;

    glm::mat4 globalMatrix = parentMatrix;
    TransformComponent* trans = go->GetComponent<TransformComponent>();

    if (trans)
    {
        globalMatrix = parentMatrix * trans->GetLocalModelMatrix();

        MeshComponent* mesh = go->GetComponent<MeshComponent>();
        if (mesh && mesh->GetPrimitive() && go != owner)
        {
            Primitive* prim = mesh->GetPrimitive();

            // Transformamos el rayo una sola vez al espacio local del GameObject
            glm::mat4 invModel = glm::inverse(globalMatrix);
            glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f));
            glm::vec3 localDir = glm::normalize(glm::vec3(invModel * glm::vec4(rayDir, 0.0f)));

            // Damos un pequeño margen (padding) por si es un Plane con grosor 0 en el eje Y
            glm::vec3 boxMin = prim->GetBoundsMin() - glm::vec3(0.001f);
            glm::vec3 boxMax = prim->GetBoundsMax() + glm::vec3(0.001f);

            float aabbDist = 0.0f;
            if (RayAABBIntersection(localOrigin, localDir, boxMin, boxMax, aabbDist))
            {
                const auto& vertices = prim->GetVertices();
                const auto& indices = prim->GetIndices();

                if (!indices.empty())
                {
                    for (size_t i = 0; i + 2 < indices.size(); i += 3)
                    {
                        const glm::vec3& v0 = vertices[indices[i]].position;
                        const glm::vec3& v1 = vertices[indices[i + 1]].position;
                        const glm::vec3& v2 = vertices[indices[i + 2]].position;

                        float localHitDist = 0.0f;
                        if (RayTriangleIntersection(localOrigin, localDir, v0, v1, v2, localHitDist))
                        {
                            glm::vec3 localHitPoint = localOrigin + localDir * localHitDist;
                            glm::vec3 worldHitPoint = glm::vec3(globalMatrix * glm::vec4(localHitPoint, 1.0f));
                            float worldDist = glm::length(worldHitPoint - rayOrigin);

                            if (worldDist < minDistance)
                            {
                                minDistance = worldDist;
                                closestObject = go;
                            }
                        }
                    }
                }
                else
                {
                    for (size_t i = 0; i + 2 < vertices.size(); i += 3)
                    {
                        const glm::vec3& v0 = vertices[i].position;
                        const glm::vec3& v1 = vertices[i + 1].position;
                        const glm::vec3& v2 = vertices[i + 2].position;

                        float localHitDist = 0.0f;
                        if (RayTriangleIntersection(localOrigin, localDir, v0, v1, v2, localHitDist))
                        {
                            glm::vec3 localHitPoint = localOrigin + localDir * localHitDist;
                            glm::vec3 worldHitPoint = glm::vec3(globalMatrix * glm::vec4(localHitPoint, 1.0f));
                            float worldDist = glm::length(worldHitPoint - rayOrigin);

                            if (worldDist < minDistance)
                            {
                                minDistance = worldDist;
                                closestObject = go;
                            }
                        }
                    }
                }
            }
        }
    }

    for (GameObject* child : go->GetChildren())
    {
        CheckRaycastRecursive(child, rayOrigin, rayDir, closestObject, minDistance, globalMatrix);
    }
}

void CameraComponent::PerformRaycast(float mX, float mY, float width, float height)
{
    if (width <= 0.0f || height <= 0.0f || !transform) return;

    Input* input = Engine::GetInstance().input.get();
    if (input && (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT)) return;

    float x = (2.0f * mX) / width - 1.0f;
    float y = 1.0f - (2.0f * mY) / height;

    glm::vec4 ray_clip = glm::vec4(x, y, -1.0f, 1.0f);
    float aspectRatio = width / height;
    glm::mat4 projection = GetProjectionMatrix(aspectRatio);

    glm::vec4 ray_eye = glm::inverse(projection) * ray_clip;
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    glm::mat4 view = GetViewMatrix();
    glm::vec3 ray_wor = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
    glm::vec3 rayOrigin = transform->GetPosition();

    Scene* scene = Engine::GetInstance().scene.get();
    if (!scene) return;

    GameObject* root = scene->GetRoot();
    GameObject* closestObj = nullptr;
    float minDistance = FLT_MAX;

    if (root)
    {
        CheckRaycastRecursive(root, rayOrigin, ray_wor, closestObj, minDistance, glm::mat4(1.0f));
    }

    scene->SetSelected(closestObj);
    if (closestObj)
    {
        LOG("[Scene] GameObject seleccionado por Raycast: '%s'", closestObj->GetName().c_str());
    }
}