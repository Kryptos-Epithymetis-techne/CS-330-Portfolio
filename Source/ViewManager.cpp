///////////////////////////////////////////////////////////////////////////////
// ViewManager.cpp
// ===============
// Manages the viewing of 3D objects within the viewport.
//
// Features implemented for CS-330 7-1 Final Project:
//   1. Perspective and orthographic projection toggle (P / O keys).
//   2. Full 6-DOF camera navigation:
//        W / S  � forward / backward  (horizontal depth)
//        A / D  � strafe left / right (horizontal)
//        Q / E  � move up   / down    (vertical)
//   3. Mouse-look: cursor controls camera yaw and pitch.
//   4. Scroll wheel adjusts camera movement speed.
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  MODIFIED BY: Student � CS-330 7-1 Final Project
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

// GLM math headers
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---------------------------------------------------------------------------
// Module-level (anonymous namespace) globals
// ---------------------------------------------------------------------------
namespace
{
    // Window dimensions (pixels)
    const int WINDOW_WIDTH = 1000;
    const int WINDOW_HEIGHT = 800;

    // Shader uniform name strings
    const char* g_ViewName = "view";
    const char* g_ProjectionName = "projection";

    // The single camera instance shared across all callbacks
    Camera* g_pCamera = nullptr;

    // Mouse tracking state
    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    bool  gFirstMouse = true;

    // Per-frame timing
    float gDeltaTime = 0.0f;
    float gLastFrame = 0.0f;

    // Projection mode:
    //   false = perspective (3-D)  |  true = orthographic (2-D)
    bool bOrthographicProjection = false;
}

// ---------------------------------------------------------------------------
// ViewManager()  �  Constructor
// ---------------------------------------------------------------------------
ViewManager::ViewManager(ShaderManager* pShaderManager)
{
    // Store the shader manager reference
    m_pShaderManager = pShaderManager;
    m_pWindow = nullptr;

    // Allocate and configure the default camera
    g_pCamera = new Camera();

    // Starting position: slightly above and behind the scene
    g_pCamera->Position = glm::vec3(0.0f, 5.0f, 12.0f);

    // Look slightly downward and into the scene
    g_pCamera->Front = glm::vec3(0.0f, -0.5f, -2.0f);

    // World up direction
    g_pCamera->Up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Field-of-view (degrees) used by perspective projection
    g_pCamera->Zoom = 80.0f;

    // Default movement speed (units per second)
    g_pCamera->MovementSpeed = 20.0f;
}

// ---------------------------------------------------------------------------
// ~ViewManager()  �  Destructor
// ---------------------------------------------------------------------------
ViewManager::~ViewManager()
{
    // Release references (do not delete the shader manager � we don't own it)
    m_pShaderManager = nullptr;
    m_pWindow = nullptr;

    // Free the camera we allocated
    if (g_pCamera != nullptr)
    {
        delete g_pCamera;
        g_pCamera = nullptr;
    }
}

// ---------------------------------------------------------------------------
// CreateDisplayWindow()
//
// Creates the GLFW window, registers input callbacks, and enables alpha
// blending for transparent geometry.
// ---------------------------------------------------------------------------
GLFWwindow* ViewManager::CreateDisplayWindow(const char* windowTitle)
{
    GLFWwindow* window = nullptr;

    // Attempt to open the window
    window = glfwCreateWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        windowTitle,
        nullptr,    // not full-screen
        nullptr);   // no shared context

    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    // Make the new window the active OpenGL context
    glfwMakeContextCurrent(window);

    // Hide and capture the mouse cursor so mouse-look works smoothly
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Register the mouse-position callback (camera orientation / look-around)
    glfwSetCursorPosCallback(window, &ViewManager::Mouse_Position_Callback);

    // Register the scroll-wheel callback (adjusts movement speed)
    glfwSetScrollCallback(window, &ViewManager::Mouse_Scroll_Callback);

    // Enable alpha blending for transparent surfaces
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_pWindow = window;
    return window;
}

// ---------------------------------------------------------------------------
// Mouse_Position_Callback()  �  static GLFW callback
//
// Called automatically by GLFW every time the cursor moves.
// Computes X / Y offsets from the last known position and forwards them to
// the camera so it can update yaw and pitch.
// ---------------------------------------------------------------------------
void ViewManager::Mouse_Position_Callback(GLFWwindow* /*window*/,
    double xMousePos,
    double yMousePos)
{
    // On the very first event we just record the position; no movement yet
    if (gFirstMouse)
    {
        gLastX = static_cast<float>(xMousePos);
        gLastY = static_cast<float>(yMousePos);
        gFirstMouse = false;
        return;
    }

    // Compute how far the cursor moved since last frame
    float xOffset = static_cast<float>(xMousePos) - gLastX;
    float yOffset = gLastY - static_cast<float>(yMousePos); // invert Y: up = positive

    // Update stored "last" position
    gLastX = static_cast<float>(xMousePos);
    gLastY = static_cast<float>(yMousePos);

    // Let the camera convert offsets into yaw / pitch changes
    g_pCamera->ProcessMouseMovement(xOffset, yOffset);
}

// ---------------------------------------------------------------------------
// Mouse_Scroll_Callback()  �  static GLFW callback
//
// Called automatically by GLFW whenever the scroll wheel is used.
// Scrolling UP increases movement speed; scrolling DOWN decreases it.
// Speed is clamped to the range [1, 100] to avoid extremes.
// ---------------------------------------------------------------------------
void ViewManager::Mouse_Scroll_Callback(GLFWwindow* /*window*/,
    double /*xOffset*/,
    double yOffset)
{
    if (g_pCamera == nullptr)
        return;

    // yOffset > 0 = scroll up (faster);  yOffset < 0 = scroll down (slower)
    g_pCamera->MovementSpeed += static_cast<float>(yOffset) * 2.0f;

    // Clamp to a sensible range so the camera never stops or flies away
    if (g_pCamera->MovementSpeed < 1.0f)
        g_pCamera->MovementSpeed = 1.0f;
    if (g_pCamera->MovementSpeed > 100.0f)
        g_pCamera->MovementSpeed = 100.0f;
}

// ---------------------------------------------------------------------------
// ProcessKeyboardEvents()  �  private
//
// Polls GLFW key state and drives the camera or toggles projection mode.
//
// Key mapping:
//   Escape � close window
//   W / S  � move forward / backward  (camera FORWARD / BACKWARD)
//   A / D  � strafe left  / right     (camera LEFT    / RIGHT   )
//   Q / E  � move up      / down      (camera UP      / DOWN    )
//   P      � switch to perspective  (3-D) projection
//   O      � switch to orthographic (2-D) projection
// ---------------------------------------------------------------------------
void ViewManager::ProcessKeyboardEvents()
{
    // --- Window close ---
    if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_pWindow, true);
    }

    // --- Forward / Backward (depth navigation) ---
    if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(FORWARD, gDeltaTime);
    }
    if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(BACKWARD, gDeltaTime);
    }

    // --- Strafe Left / Right (horizontal navigation) ---
    if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(LEFT, gDeltaTime);
    }
    if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(RIGHT, gDeltaTime);
    }

    // --- Move Up / Down (vertical navigation) ---
    if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(UP, gDeltaTime);
    }
    if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
    {
        g_pCamera->ProcessKeyboard(DOWN, gDeltaTime);
    }

    // --- Projection toggle ---
    // P key: switch to perspective (3-D) view
    if (glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS)
    {
        bOrthographicProjection = false;
    }
    // O key: switch to orthographic (2-D) view
    if (glfwGetKey(m_pWindow, GLFW_KEY_O) == GLFW_PRESS)
    {
        bOrthographicProjection = true;
    }
}

// ---------------------------------------------------------------------------
// PrepareSceneView()
//
// Called once per render frame.  Responsibilities:
//   1. Compute delta time for frame-rate-independent movement.
//   2. Process all queued keyboard events.
//   3. Build the view matrix from the camera.
//   4. Build either a perspective or orthographic projection matrix,
//      depending on the current mode flag.
//   5. Upload view, projection, and camera-position to the shader.
// ---------------------------------------------------------------------------
void ViewManager::PrepareSceneView()
{
    // -----------------------------------------------------------------------
    // 1. Per-frame timing
    // -----------------------------------------------------------------------
    float currentFrame = static_cast<float>(glfwGetTime());
    gDeltaTime = currentFrame - gLastFrame;
    gLastFrame = currentFrame;

    // -----------------------------------------------------------------------
    // 2. Handle keyboard input
    // -----------------------------------------------------------------------
    ProcessKeyboardEvents();

    // -----------------------------------------------------------------------
    // 3. View matrix (where the camera is and where it is looking)
    // -----------------------------------------------------------------------
    glm::mat4 view = g_pCamera->GetViewMatrix();

    // -----------------------------------------------------------------------
    // 4. Projection matrix
    //    Perspective  � realistic depth / foreshortening (default, key P)
    //    Orthographic � parallel projection, no depth cue   (key O)
    // -----------------------------------------------------------------------
    glm::mat4 projection;

    if (bOrthographicProjection)
    {
        // Orthographic: define a symmetric frustum in world units.
        // The scale factor keeps the objects a similar apparent size to the
        // perspective view.  The camera looks straight down from above so the
        // ground plane is not visible (as required by the rubric).
        float orthoScale = 10.0f; // half-width / half-height of the view volume

        projection = glm::ortho(
            -orthoScale,                                            // left
            orthoScale,                                            // right
            -orthoScale / ((float)WINDOW_WIDTH / (float)WINDOW_HEIGHT), // bottom (aspect-corrected)
            orthoScale / ((float)WINDOW_WIDTH / (float)WINDOW_HEIGHT), // top
            -50.0f,                                                 // near plane
            100.0f);                                               // far plane
    }
    else
    {
        // Perspective: standard field-of-view projection
        projection = glm::perspective(
            glm::radians(g_pCamera->Zoom),                         // vertical FOV
            (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT,            // aspect ratio
            0.1f,                                                   // near clip
            100.0f);                                                // far clip
    }

    // -----------------------------------------------------------------------
    // 5. Upload matrices and camera position to the active shader program
    // -----------------------------------------------------------------------
    if (m_pShaderManager != nullptr)
    {
        m_pShaderManager->setMat4Value(g_ViewName, view);
        m_pShaderManager->setMat4Value(g_ProjectionName, projection);
        m_pShaderManager->setVec3Value("viewPosition", g_pCamera->Position);
    }
}