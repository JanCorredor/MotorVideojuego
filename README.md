# Maruja Engine

**Maruja** es un motor de videojuegos desarrollado en C++ para la asignatura *Motores de Videojuegos* (3º año, Universidad CITM). Esta primera versión funciona como un visualizador de geometría  e incorpora un sistema de ventanas jerárquicas y renderizado en tiempo real.

🔗 **Repositorio Oficial:** [MotorVideojuego en GitHub](https://github.com/JanCorredor/MotorVideojuego)

## 👥 Colaboradores
- **Rubén Mateo (Rubenmg0):** [https://github.com/Rubenmg9](https://github.com/Rubenmg9)
- **Marc Jiménez (MarcJM-00):** [https://github.com/MarcJM-00](https://github.com/MarcJM-00)
- **Jan Corredor (JanCorredor):** [https://github.com/JanCorredor](https://github.com/JanCorredor)

---

## 🚀 Funcionalidades Completadas
Se han implementado todos los requisitos obligatorios de la Entrega 1:
- **Mallas y Texturas:** Carga automática de `baker_house.fbx` al iniciar. Soporte *Drag & Drop* para archivos `.fbx` (geometría) desde cualquier ruta del disco y `.dds` / `.png` (texturas aplicables al GameObject seleccionado).
- **Arquitectura de GameObjects:** Sistema de entidades con componentes `Transform`, `Mesh` y `Textura`. 
- **Cámara Orbital (Estilo Unity):** Navegación fluida por la escena.
- **Editor de Interfaz (ImGui):**
  - **Escena:** Renderizado principal del modelo.
  - **Jerarquía e Inspector:** Ventanas interactivas para listar GameObjects y visualizar propiedades (Transform, normales de Mesh, path, tamaño y visualización *checker* de Texturas).
  - **Consola:** Registro de flujo, inicialización de librerías externas y logs de proceso de ASSIMP y DevIL.
  - **Configuración:** Gráficas de FPS, consumo de memoria y versiones del software de la base técnica (SDL3, OpenGL, DevIL).
  - **Barra de Herramientas Principal:** Menús completos (File, View, Help con accesos al repositorio oficial y About).

---

## 🎮 Controles de Uso

### Cámara
- **Click Derecho (Mantener) + WASD:** Movimiento libre (FPS-like) y free look around.
- **Rueda del Ratón:** Zoom.
- **Alt + Click Izquierdo (Mantener):** Orbitar alrededor de la geometría seleccionada.
- **Tecla `F`:** Focus en la geometría seleccionada.
- **Shift (Mantener):** Duplica la velocidad de movimiento de la cámara.

### Importación de Archivos (Drag & Drop)
- Arrastra un archivo `.fbx` a la ventana para crear un nuevo GameObject con su malla correspondiente.
- Arrastra un archivo `.png` o `.dds` para texturizar el GameObject que esté seleccionado en la jerarquía actualmente.

---

## ⭐ Funcionalidades Extra
- Gestión avanzada de jerarquía (Parent-Child) entre GameObjects.
- Sistema de *grid* (cuadrícula) y *axis* (ejes) activables.
- Redimensionamiento de ventanas interactivo y en tiempo real.
- Mouse Picking + Outline en el gameobject seleccionado
- Wireframe
- Missing texture
- Z-buffer
- Render options: Depht Test + Face Culling + Depth Test.
- Interfaz modular implementada con un importador de módulos dedicado.

---

## 📝 Notas para el Profesor
- El panel de z-buffer aparece si lo activas en el apartado view
- Si subes un fbx y le asignas una textura invalida, se asigna una missing texture
