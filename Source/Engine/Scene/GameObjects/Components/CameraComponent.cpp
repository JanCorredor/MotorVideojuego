#include "CameraComponent.h"
#include "../../../Engine.h"
#include "../../../Input/Input.h"
#include "../../../Window/Window.h"
#include "../../../Render/Render.h"
#include "../GameObject.h"
#include "../../Scene.h"
#include "MeshComponent.h"
#include "../../../Geometry/Ray.h"
#include "../../../../Utils/Log.h"
#include "../../../../Utils/JsonHelpers.h"
#include <algorithm>
#include <stack>
#include <unordered_set>

CameraComponent::CameraComponent(GameObject* owner) : Component(COMPONENT_CAMERA, owner)
{
    transform = owner ? owner->GetComponent<TransformComponent>() : nullptr;
    UpdateCameraVectors();
}

std::unique_ptr<Component> CameraComponent::Clone(GameObject* newOwner) const
{
    auto clone = std::make_unique<CameraComponent>(newOwner);
    clone->fov = this->fov;
    clone->nearPlane = this->nearPlane;
    clone->farPlane = this->farPlane;
    clone->movementSpeed = this->movementSpeed;
    clone->mouseSensitivity = this->mouseSensitivity;
    clone->orbitDistance = this->orbitDistance;
    clone->yaw = this->yaw;
    clone->pitch = this->pitch;
    clone->UpdateCameraVectors();
    if (!active) clone->Disable();
    return clone;
}

void CameraComponent::Save(nlohmann::json& out) const
{
    out["active"] = active;
    out["fov"] = fov;
    out["near"] = nearPlane;
    out["far"] = farPlane;
    out["movementSpeed"] = movementSpeed;
    out["mouseSensitivity"] = mouseSensitivity;
    out["orbitDistance"] = orbitDistance;
    out["yaw"] = yaw;
    out["pitch"] = pitch;
}

void CameraComponent::Load(const nlohmann::json& in)
{
    active = JsonHelpers::Get(in, "active", true);
    fov = JsonHelpers::Get(in, "fov", 45.0f);
    nearPlane = JsonHelpers::Get(in, "near", 0.1f);
    farPlane = JsonHelpers::Get(in, "far", 100.0f);
    movementSpeed = JsonHelpers::Get(in, "movementSpeed", 5.0f);
    mouseSensitivity = JsonHelpers::Get(in, "mouseSensitivity", 0.1f);
    orbitDistance = JsonHelpers::Get(in, "orbitDistance", 5.0f);
    yaw = JsonHelpers::Get(in, "yaw", -90.0f);
    pitch = JsonHelpers::Get(in, "pitch", 0.0f);
    transform = owner ? owner->GetComponent<TransformComponent>() : nullptr;
    UpdateCameraVectors();
}

void CameraComponent::Update(float dt)
{
    if (!transform && owner)
    {
        transform = owner->GetComponent<TransformComponent>();
    }
    if (!transform) return;

    Input* input = Engine::GetInstance().input.get();
    Scene* scene = Engine::GetInstance().scene.get();
    if (scene && owner != scene->GetMainCameraObject()) return;
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
    bool mmbDown = (input->GetMouseButton(SDL_BUTTON_MIDDLE) == KEY_DOWN);
    bool mmbHeld = (mmbDown || input->GetMouseButton(SDL_BUTTON_MIDDLE) == KEY_REPEAT);
    bool altHeld = (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT ||
                    input->GetKey(SDL_SCANCODE_RALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_RALT) == KEY_REPEAT);
    bool lmbDown = (input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_DOWN);
    bool lmbHeld = (lmbDown || input->GetMouseButton(SDL_BUTTON_LEFT) == KEY_REPEAT);

    // Solo iniciamos el control de cámara si el clic comienza dentro del viewport de Scene
    if ((rmbDown || (altHeld && lmbDown) || mmbDown) && viewportHovered && !isControllingCamera)
    {
        isControllingCamera = true;
    }

    if (!rmbHeld && !(altHeld && lmbHeld) && !mmbHeld)
    {
        isControllingCamera = false;
    }

    bool isPanning = isControllingCamera && mmbHeld; // Alt + MMB (Maya/Unity) o MMB mantenido (Unity)
    bool isOrbiting = isControllingCamera && !isPanning && altHeld && lmbHeld && selectedObject != nullptr;
    bool isFreeLooking = isControllingCamera && !isPanning && (rmbHeld || (altHeld && lmbHeld && selectedObject == nullptr));
    bool activeControl = isFreeLooking || isOrbiting || isPanning;
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

    bool shiftHeld = (input->GetKey(SDL_SCANCODE_LSHIFT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LSHIFT) == KEY_REPEAT);
    float currentSpeed = movementSpeed;
    if (shiftHeld)
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

    // Botón derecho o Alt + Click Izquierdo (sin objeto seleccionado): Free look y WASD/QE
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

    // Alt + Click Izquierdo (con objeto seleccionado): Orbitar objeto seleccionado
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

    // Alt + Click Rueda del Ratón (o Click Rueda): Pan lateral y vertical (estilo Maya / Unity)
    if (isPanning)
    {
        glm::vec2 mouseDelta = input->GetMouseDelta();
        float panSpeed = 0.015f * std::max(1.0f, orbitDistance * 0.25f) * (shiftHeld ? 2.0f : 1.0f);

        glm::vec3 pos = transform->GetPosition();
        pos -= right * (mouseDelta.x * panSpeed);
        pos += up * (mouseDelta.y * panSpeed);
        transform->SetPosition(pos);
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
    if (!transform && owner)
    {
        transform = owner->GetComponent<TransformComponent>();
    }
    if (!transform) return glm::mat4(1.0f);

    Scene* scene = Engine::GetInstance().scene.get();
    // Si es la cámara de escena (Main Camera), usamos la matriz de su TransformComponent (posición + rotación del Inspector)
    if (scene && owner != scene->GetMainCameraObject())
    {
        glm::mat4 worldMat = owner->GetWorldMatrix();
        glm::vec3 camPos = glm::vec3(worldMat[3]);
        glm::vec3 camForward = glm::normalize(-glm::vec3(worldMat[2])); // -Z local es hacia adelante en OpenGL
        glm::vec3 camUp = glm::normalize(glm::vec3(worldMat[1]));      // +Y local es hacia arriba
        return glm::lookAt(camPos, camPos + camForward, camUp);
    }

    // Si es la EditorCamera, usamos los vectores controlados por el ratón (yaw/pitch)
    glm::vec3 pos = transform->GetPosition();
    return glm::lookAt(pos, pos + front, up);
}

glm::mat4 CameraComponent::GetProjectionMatrix(float aspectRatio)
{
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

void CameraComponent::FocusOnObject(GameObject* target)
{
    if (!transform && owner) transform = owner->GetComponent<TransformComponent>();
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
    if (!transform && owner) transform = owner->GetComponent<TransformComponent>();
    yaw = -90.0f;
    pitch = 0.0f;
    fov = 45.0f;
    UpdateCameraVectors();
    if (transform)
    {
        transform->SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
    }
}

// Recorrido iterativo con stack + aceleración mediante Octree: Transforma el rayo a Local Space -> Prueba AABB -> Prueba Triángulos
void CameraComponent::CheckRaycastRecursive(GameObject* rootGo, glm::vec3 rayOrigin, glm::vec3 rayDir, GameObject*& closestObject, float& minDistance, const glm::mat4& rootParentMatrix)
{
    if (!rootGo || !rootGo->IsActive()) return;

    Scene* scene = Engine::GetInstance().scene.get();
    Render* render = Engine::GetInstance().render.get();
    SpacePartitionMode partitionMode = render ? render->GetSettings().partitionMode : SpacePartitionMode::None;

    bool useTreeFilter = false;
    std::unordered_set<GameObject*> staticCandidatesSet;

    if (scene && partitionMode != SpacePartitionMode::None)
    {
        std::vector<GameObject*> treeHits;
        if (partitionMode == SpacePartitionMode::Octree)
        {
            scene->GetOctree().QueryRay(rayOrigin, rayDir, treeHits);
        }
        staticCandidatesSet.insert(treeHits.begin(), treeHits.end());
        useTreeFilter = true;
    }

    struct RaycastNode
    {
        GameObject* go;
        glm::mat4 parentMatrix;
    };

    std::stack<RaycastNode> stack;
    stack.push({ rootGo, rootParentMatrix });

    while (!stack.empty())
    {
        RaycastNode current = stack.top();
        stack.pop();

        GameObject* go = current.go;
        if (!go || !go->IsActive()) continue;

        glm::mat4 globalMatrix = current.parentMatrix;
        TransformComponent* trans = go->GetComponent<TransformComponent>();

        if (trans)
        {
            globalMatrix = current.parentMatrix * trans->GetLocalModelMatrix();

            MeshComponent* mesh = go->GetComponent<MeshComponent>();
            if (mesh && mesh->IsActive() && mesh->GetPrimitive() && go != owner)
            {
                // Si el objeto es Static y hay Space Partitioning activo, descartarlo en O(1) si el árbol ya filtró su rama
                if (!useTreeFilter || !go->IsStatic() || staticCandidatesSet.find(go) != staticCandidatesSet.end())
                {
                    const Primitive* prim = mesh->GetPrimitive();

                    // Transformamos el rayo una sola vez al espacio local del GameObject
                    glm::mat4 invModel = glm::inverse(globalMatrix);
                    const Ray localRay(glm::vec3(invModel * glm::vec4(rayOrigin, 1.0f)), glm::vec3(invModel * glm::vec4(rayDir, 0.0f)));

                    // Damos un pequeño margen (padding) por si es un Plane con grosor 0 en el eje Y
                    const AABB localBox(prim->GetBoundsMin() - glm::vec3(0.001f), prim->GetBoundsMax() + glm::vec3(0.001f));

                    float aabbDist = 0.0f;
                    if (Intersection::RayAABB(localRay, localBox, aabbDist))
                    {
                        const std::vector<Vertex>& vertices = prim->GetVertices();
                        const std::vector<unsigned int>& indices = prim->GetIndices();

                        if (!indices.empty())
                        {
                            for (size_t i = 0; i + 2 < indices.size(); i += 3)
                            {
                                const glm::vec3& v0 = vertices[indices[i]].position;
                                const glm::vec3& v1 = vertices[indices[i + 1]].position;
                                const glm::vec3& v2 = vertices[indices[i + 2]].position;

                                float localHitDist = 0.0f;
                                if (Intersection::RayTriangle(localRay, v0, v1, v2, localHitDist))
                                {
                                    glm::vec3 localHitPoint = localRay.GetPoint(localHitDist);
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
                                if (Intersection::RayTriangle(localRay, v0, v1, v2, localHitDist))
                                {
                                    glm::vec3 localHitPoint = localRay.GetPoint(localHitDist);
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
        }

        const std::vector<GameObject*>& children = go->GetChildren();
        for (size_t i = children.size(); i > 0; --i)
        {
            stack.push({ children[i - 1], globalMatrix });
        }
    }
}

void CameraComponent::PerformRaycast(float mX, float mY, float width, float height)
{
    if (!transform && owner) transform = owner->GetComponent<TransformComponent>();
    if (width <= 0.0f || height <= 0.0f || !transform) return;

    Input* input = Engine::GetInstance().input.get();
    if (input && (input->GetKey(SDL_SCANCODE_LALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_LALT) == KEY_REPEAT ||
                  input->GetKey(SDL_SCANCODE_RALT) == KEY_DOWN || input->GetKey(SDL_SCANCODE_RALT) == KEY_REPEAT)) return;

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