///////////////////////////////////////////////////////////////////////////////
// ViewManager.h
// ============
// Manages the viewing of 3D objects within the viewport.
// Handles camera creation, projection switching (perspective / orthographic),
// keyboard navigation (WASD + QE), mouse-look, and scroll-wheel speed control.
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  MODIFIED BY: Student - CS-330 7-1 Final Project
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShaderManager.h"
#include "camera.h"

// GLFW library
#include "GLFW/glfw3.h"

class ViewManager
{
public:
    // -----------------------------------------------------------------------
    // Constructor / Destructor
    // -----------------------------------------------------------------------
    ViewManager(ShaderManager* pShaderManager);
    ~ViewManager();

    // -----------------------------------------------------------------------
    // GLFW static callbacks (must be static so they can be registered with C)
    // -----------------------------------------------------------------------

    /// Called by GLFW whenever the mouse cursor moves.
    static void Mouse_Position_Callback(GLFWwindow* window,
        double xMousePos,
        double yMousePos);

    /// Called by GLFW whenever the mouse scroll wheel is used.
    /// Adjusts camera movement speed so the user can travel faster or slower.
    static void Mouse_Scroll_Callback(GLFWwindow* window,
        double xOffset,
        double yOffset);

    // -----------------------------------------------------------------------
    // Public interface
    // -----------------------------------------------------------------------

    /// Creates and returns the main OpenGL display window.
    GLFWwindow* CreateDisplayWindow(const char* windowTitle);

    /// Called once per frame; updates timing, processes input, and uploads
    /// the view / projection matrices to the shader.
    void PrepareSceneView();

private:
    // Pointer to the shared shader manager
    ShaderManager* m_pShaderManager;

    // Active GLFW display window
    GLFWwindow* m_pWindow;

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /// Polls GLFW for keyboard state and moves the camera accordingly.
    /// Keys handled:
    ///   W / S  - forward / backward
    ///   A / D  - strafe left / right
    ///   Q / E  - move up / move down
    ///   P      - switch to perspective projection
    ///   O      - switch to orthographic projection
    ///   Escape - close the window
    void ProcessKeyboardEvents();
};