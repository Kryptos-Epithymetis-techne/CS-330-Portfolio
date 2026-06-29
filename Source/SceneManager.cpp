///////////////////////////////////////////////////////////////////////////////
// SceneManager.cpp
// ================
// Implements the SceneManager class, responsible for:
//   - Loading and binding OpenGL textures.
//   - Defining Phong materials for each object.
//   - Configuring scene lighting (directional + point lights).
//   - Setting per-object shader uniforms (model matrix, color/texture,
//     material, lighting enable flag).
//   - Preparing and rendering the full 3D scene.
//
// LIGHTING SETUP
// --------------
//   Light 1 (directional) - warm white overhead sun/ceiling light.
//              Provides broad ambient fill so nothing falls into complete shadow.
//   Light 2 (point light 0) - soft cool fill light from the left side.
//              Secondary source that illuminates areas the directional misses.
//
// PHONG MATERIALS
// ---------------
//   wood        - moderate diffuse, low specular (matte surface)
//   dark_metal  - low diffuse, high specular, high shininess (polished metal)
//   brushed_met - moderate diffuse, moderate specular (brushed aluminium)
//   gold        - warm diffuse, strong specular (glossy gold dome)
//   ceramic     - moderate-high diffuse, bright specular (glazed mug)
//   book_cover  - high diffuse, low specular (matte cloth/leather cover)
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  MODIFIED BY: Student - CS-330 8-1 Final Project (lighting milestone)
//  Course: CS-330 Computational Graphics and Visualization
//  INITIAL VERSION: November 1, 2023
//  LAST REVISED:    December 2024
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

// ---------------------------------------------------------------------------
// Shader uniform name strings (file-private)
// ---------------------------------------------------------------------------
namespace
{
    const char* g_ModelName = "model";
    const char* g_ColorValueName = "objectColor";
    const char* g_TextureValueName = "objectTexture";
    const char* g_UseTextureName = "bUseTexture";
    const char* g_UseLightingName = "bUseLighting";
}

// ---------------------------------------------------------------------------
// SceneManager()  -  Constructor
// ---------------------------------------------------------------------------
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
    m_pShaderManager = pShaderManager;
    m_basicMeshes = new ShapeMeshes();
    m_loadedTextures = 0;
}

// ---------------------------------------------------------------------------
// ~SceneManager()  -  Destructor
// ---------------------------------------------------------------------------
SceneManager::~SceneManager()
{
    m_pShaderManager = nullptr;
    delete m_basicMeshes;
    m_basicMeshes = nullptr;
}

// ---------------------------------------------------------------------------
// CreateGLTexture()
//
// Loads an image file into an OpenGL texture object, configures wrapping and
// filtering, generates mipmaps, and registers it under a lookup tag.
//
// Parameters:
//   filename - path to the image file (relative to the working directory)
//   tag      - unique string key used to retrieve the texture later
//
// Returns true on success, false if the file could not be loaded.
// ---------------------------------------------------------------------------
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
    int    width = 0;
    int    height = 0;
    int    colorChannels = 0;
    GLuint textureID = 0;

    // stb_image stores rows top-to-bottom; OpenGL expects bottom-to-top
    stbi_set_flip_vertically_on_load(true);

    unsigned char* image = stbi_load(filename, &width, &height, &colorChannels, 0);

    if (image)
    {
        std::cout << "Loaded texture: " << filename
            << " (" << width << "x" << height
            << ", " << colorChannels << " ch)" << std::endl;

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // Tile in both axes so textures repeat across large surfaces
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Linear filtering for smooth rendering at any distance
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (colorChannels == 3)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,
                width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        }
        else if (colorChannels == 4)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        }
        else
        {
            std::cout << "Unsupported channel count: " << colorChannels << std::endl;
            stbi_image_free(image);
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Store in the lookup table
        m_textureIDs[m_loadedTextures].ID = textureID;
        m_textureIDs[m_loadedTextures].tag = tag;
        m_loadedTextures++;

        return true;
    }

    std::cout << "Could not load texture: " << filename << std::endl;
    return false;
}

// ---------------------------------------------------------------------------
// BindGLTextures()
//
// Binds every loaded texture to its corresponding GL_TEXTUREn slot so the
// fragment shader can sample any of them by slot index.
// ---------------------------------------------------------------------------
void SceneManager::BindGLTextures()
{
    for (int i = 0; i < m_loadedTextures; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
    }
}

// ---------------------------------------------------------------------------
// DestroyGLTextures()
//
// Releases all GPU texture memory allocated by CreateGLTexture().
// ---------------------------------------------------------------------------
void SceneManager::DestroyGLTextures()
{
    for (int i = 0; i < m_loadedTextures; i++)
    {
        glDeleteTextures(1, &m_textureIDs[i].ID);
    }
}

// ---------------------------------------------------------------------------
// FindTextureID() / FindTextureSlot()
//
// Lookup helpers that search the texture registry by tag and return either
// the OpenGL texture object ID or the texture unit slot index.
// ---------------------------------------------------------------------------
int SceneManager::FindTextureID(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; i++)
        if (m_textureIDs[i].tag == tag)
            return m_textureIDs[i].ID;
    return -1;
}

int SceneManager::FindTextureSlot(std::string tag)
{
    for (int i = 0; i < m_loadedTextures; i++)
        if (m_textureIDs[i].tag == tag)
            return i;
    return -1;
}

// ---------------------------------------------------------------------------
// FindMaterial()
//
// Searches the material list for the given tag.  Copies the material data
// into 'material' and returns true on success.
// ---------------------------------------------------------------------------
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
    for (int i = 0; i < (int)m_objectMaterials.size(); i++)
    {
        if (m_objectMaterials[i].tag == tag)
        {
            material.diffuseColor = m_objectMaterials[i].diffuseColor;
            material.specularColor = m_objectMaterials[i].specularColor;
            material.shininess = m_objectMaterials[i].shininess;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// SetTransformations()
//
// Builds the model matrix (Scale -> RotX -> RotY -> RotZ -> Translate) and
// uploads it to the "model" shader uniform.
// ---------------------------------------------------------------------------
void SceneManager::SetTransformations(
    glm::vec3 scaleXYZ,
    float     XrotationDegrees,
    float     YrotationDegrees,
    float     ZrotationDegrees,
    glm::vec3 positionXYZ)
{
    glm::mat4 scale = glm::scale(scaleXYZ);
    glm::mat4 rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1, 0, 0));
    glm::mat4 rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0, 1, 0));
    glm::mat4 rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0, 0, 1));
    glm::mat4 translation = glm::translate(positionXYZ);

    glm::mat4 modelView = translation * rotationZ * rotationY * rotationX * scale;

    if (m_pShaderManager)
        m_pShaderManager->setMat4Value(g_ModelName, modelView);
}

// ---------------------------------------------------------------------------
// SetShaderColor()
//
// Uploads a flat RGBA color and disables texture sampling for the next draw.
// ---------------------------------------------------------------------------
void SceneManager::SetShaderColor(
    float r, float g, float b, float a)
{
    glm::vec4 color(r, g, b, a);
    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(g_UseTextureName, false);
        m_pShaderManager->setVec4Value(g_ColorValueName, color);
    }
}

// ---------------------------------------------------------------------------
// SetShaderTexture()
//
// Looks up the texture slot for the given tag, enables texture sampling, and
// uploads the slot index to the sampler uniform.
// ---------------------------------------------------------------------------
void SceneManager::SetShaderTexture(std::string textureTag)
{
    if (m_pShaderManager)
    {
        m_pShaderManager->setIntValue(g_UseTextureName, true);
        int slot = FindTextureSlot(textureTag);
        m_pShaderManager->setSampler2DValue(g_TextureValueName, slot);
    }
}

// ---------------------------------------------------------------------------
// SetTextureUVScale()
//
// Uploads the UV tiling multiplier so textures can repeat across surfaces.
// ---------------------------------------------------------------------------
void SceneManager::SetTextureUVScale(float u, float v)
{
    if (m_pShaderManager)
        m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
}

// ---------------------------------------------------------------------------
// SetShaderMaterial()
//
// Looks up the named material and uploads its diffuse color, specular color,
// and shininess to the shader's material uniform struct.
// ---------------------------------------------------------------------------
void SceneManager::SetShaderMaterial(std::string materialTag)
{
    if (!m_objectMaterials.empty())
    {
        OBJECT_MATERIAL mat;
        if (FindMaterial(materialTag, mat))
        {
            m_pShaderManager->setVec3Value("material.diffuseColor", mat.diffuseColor);
            m_pShaderManager->setVec3Value("material.specularColor", mat.specularColor);
            m_pShaderManager->setFloatValue("material.shininess", mat.shininess);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// STUDENT-EDITABLE SECTION
///////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------------
// LoadSceneTextures()  [private helper]
//
// Loads every texture image needed by the scene and binds them to GL texture
// slots.  Called once from PrepareScene().
//
// Textures:
//   "wood"         - warm wood grain for the desk surface plane
//   "dark_metal"   - dark brushed metal for the lamp base and pivot joint
//   "brushed_metal"- silver brushed aluminium for the arm cylinders
//   "gold"         - warm gold finish for the lamp head sphere
// ---------------------------------------------------------------------------
void SceneManager::LoadSceneTextures()
{
    CreateGLTexture("textures/wood_floor.jpg", "wood");
    CreateGLTexture("textures/metal_dark.jpg", "dark_metal");
    CreateGLTexture("textures/metal_brushed.jpg", "brushed_metal");
    CreateGLTexture("textures/gold_metal.jpg", "gold");
    CreateGLTexture("textures/ceramic_green.jpg", "ceramic");
    CreateGLTexture("textures/book_cover.jpg", "book_cover");

    // Bind all loaded textures to slots 0-N so shaders can sample them
    BindGLTextures();
}

// ---------------------------------------------------------------------------
// DefineObjectMaterials()  [private helper]
//
// Populates the material list with Phong material definitions for each
// surface type in the scene.  Each material controls how the Phong lighting
// model splits reflected light:
//
//   diffuseColor  - base color reflected under diffuse lighting
//   specularColor - color of specular highlights
//   shininess     - Phong exponent: higher = tighter / shinier highlight
//
// These values are uploaded per-object before each draw call via
// SetShaderMaterial().
// ---------------------------------------------------------------------------
void SceneManager::DefineObjectMaterials()
{
    // --- Wood (desk / table surface) ---
    // Matte surface: moderate diffuse, very low specular so the plane reflects
    // light broadly (fulfills rubric: "shaders that reflect light off a plane").
    // Shininess kept low so reflections are wide and soft, not mirror-like.
    OBJECT_MATERIAL woodMaterial;
    woodMaterial.diffuseColor = glm::vec3(0.8f, 0.6f, 0.4f);  // warm tan
    woodMaterial.specularColor = glm::vec3(0.3f, 0.25f, 0.2f); // faint warm sheen
    woodMaterial.shininess = 16.0f;                          // soft highlight
    woodMaterial.tag = "wood";
    m_objectMaterials.push_back(woodMaterial);

    // --- Dark metal (lamp base + pivot joint torus) ---
    // Polished dark metal: low diffuse (dark appearance), strong specular
    // highlight, high shininess for a tight, mirror-like reflection.
    OBJECT_MATERIAL darkMetalMaterial;
    darkMetalMaterial.diffuseColor = glm::vec3(0.2f, 0.2f, 0.2f);  // near-black
    darkMetalMaterial.specularColor = glm::vec3(0.8f, 0.8f, 0.8f);  // bright white spec
    darkMetalMaterial.shininess = 128.0f;                         // very tight highlight
    darkMetalMaterial.tag = "dark_metal";
    m_objectMaterials.push_back(darkMetalMaterial);

    // --- Brushed aluminium (lower arm + upper arm cylinders) ---
    // Mid-range metal: moderate diffuse, moderate specular.  The slightly
    // lower shininess broadens the highlight to simulate brushed (vs polished).
    OBJECT_MATERIAL brushedMetalMaterial;
    brushedMetalMaterial.diffuseColor = glm::vec3(0.55f, 0.55f, 0.6f); // cool silver
    brushedMetalMaterial.specularColor = glm::vec3(0.7f, 0.7f, 0.7f); // medium-bright
    brushedMetalMaterial.shininess = 64.0f;                           // moderate highlight
    brushedMetalMaterial.tag = "brushed_metal";
    m_objectMaterials.push_back(brushedMetalMaterial);

    // --- Gold (lamp head sphere) ---
    // Warm glossy gold: golden diffuse tint, strong yellow-tinted specular,
    // high shininess gives the dome a focal, reflective appearance.
    OBJECT_MATERIAL goldMaterial;
    goldMaterial.diffuseColor = glm::vec3(0.9f, 0.75f, 0.1f);  // rich gold
    goldMaterial.specularColor = glm::vec3(1.0f, 0.9f, 0.4f);  // warm gold highlight
    goldMaterial.shininess = 96.0f;                             // sharp sheen
    goldMaterial.tag = "gold";
    m_objectMaterials.push_back(goldMaterial);

    // --- Ceramic (coffee mug body + handle) ---
    // Glossy ceramic glaze: fairly high diffuse so the green color reads
    // clearly, with a crisp, bright specular highlight typical of glazed
    // pottery.
    OBJECT_MATERIAL ceramicMaterial;
    ceramicMaterial.diffuseColor = glm::vec3(0.35f, 0.55f, 0.35f);  // muted sage green
    ceramicMaterial.specularColor = glm::vec3(0.6f, 0.6f, 0.6f);    // bright glaze highlight
    ceramicMaterial.shininess = 48.0f;                              // smooth, glossy
    ceramicMaterial.tag = "ceramic";
    m_objectMaterials.push_back(ceramicMaterial);

    // --- Book cover (stacked books) ---
    // Cloth/leather-bound cover: low specular, mostly diffuse so the books
    // read as soft, non-reflective objects that don't compete visually with
    // the metal lamp or glazed mug.
    OBJECT_MATERIAL bookMaterial;
    bookMaterial.diffuseColor = glm::vec3(0.5f, 0.3f, 0.15f);   // warm brown cover
    bookMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);   // nearly matte
    bookMaterial.shininess = 8.0f;                               // very soft highlight
    bookMaterial.tag = "book_cover";
    m_objectMaterials.push_back(bookMaterial);
}

// ---------------------------------------------------------------------------
// SetupSceneLights()  [private helper]
//
// Configures and uploads all light source uniforms to the fragment shader.
// Two lights are used so that no part of the scene falls into complete shadow
// (rubric requirement).
//
// Light 1 - Directional (overhead warm-white ceiling / sunlight)
//   Simulates a broad room light.  Comes from above and slightly in front.
//   Provides ambient fill across the entire scene and strong diffuse
//   illumination on top-facing surfaces (desk, lamp top).
//
// Light 2 - Point light (soft cool-white fill from the left)
//   A secondary source that wraps light around the far side of the lamp so
//   the back and left faces are not in complete darkness.  Positioned to the
//   upper-left of the scene.  Lower intensity than the key light.
// ---------------------------------------------------------------------------
void SceneManager::SetupSceneLights()
{
    // Tell the fragment shader to run the Phong lighting path
    m_pShaderManager->setBoolValue(g_UseLightingName, true);

    // ------------------------------------------------------------------
    // LIGHT 1 - Directional light (warm white overhead key light)
    // ------------------------------------------------------------------
    // Direction points downward and slightly toward the scene; the shader
    // negates it internally to get the surface-to-light vector.
    m_pShaderManager->setVec3Value("directionalLight.direction",
        glm::vec3(0.0f, -1.0f, -0.3f));

    // Ambient: low-level fill so shadowed areas are never completely black
    m_pShaderManager->setVec3Value("directionalLight.ambient",
        glm::vec3(0.25f, 0.22f, 0.18f));

    // Diffuse: warm white - the dominant light color for most surfaces
    m_pShaderManager->setVec3Value("directionalLight.diffuse",
        glm::vec3(0.9f, 0.85f, 0.75f));

    // Specular: bright near-white so shiny surfaces (metal, gold) glint
    m_pShaderManager->setVec3Value("directionalLight.specular",
        glm::vec3(1.0f, 0.95f, 0.85f));

    // Activate the directional light
    m_pShaderManager->setBoolValue("directionalLight.bActive", true);

    // ------------------------------------------------------------------
    // LIGHT 2 - Point light (cool-white fill from upper-left)
    // ------------------------------------------------------------------
    // Positioned to the upper-left of the scene, slightly in front of the
    // lamp.  Wraps softer, cooler light onto areas the directional misses.
    m_pShaderManager->setVec3Value("pointLights[0].position",
        glm::vec3(-6.0f, 8.0f, 4.0f));

    // Ambient: very slight cool tint
    m_pShaderManager->setVec3Value("pointLights[0].ambient",
        glm::vec3(0.05f, 0.05f, 0.08f));

    // Diffuse: soft cool-white fill
    m_pShaderManager->setVec3Value("pointLights[0].diffuse",
        glm::vec3(0.45f, 0.45f, 0.55f));

    // Specular: moderate cool highlight
    m_pShaderManager->setVec3Value("pointLights[0].specular",
        glm::vec3(0.4f, 0.4f, 0.5f));

    // Activate point light 0; leave pointLights[1..4] inactive
    m_pShaderManager->setBoolValue("pointLights[0].bActive", true);

    // Deactivate remaining point light slots
    m_pShaderManager->setBoolValue("pointLights[1].bActive", false);
    m_pShaderManager->setBoolValue("pointLights[2].bActive", false);
    m_pShaderManager->setBoolValue("pointLights[3].bActive", false);
    m_pShaderManager->setBoolValue("pointLights[4].bActive", false);

    // Spotlight is not used in this scene
    m_pShaderManager->setBoolValue("spotLight.bActive", false);
}

// ---------------------------------------------------------------------------
// PrepareScene()
//
// Called once at startup.  Loads textures, defines materials, configures
// lighting, and uploads all mesh geometry to the GPU.
//
// Meshes loaded:
//   Plane    - desk/table surface (3-D base plane)
//   Cylinder - lamp base, lower arm, upper arm, mug body (shared mesh)
//   Torus    - pivot joint elbow, mug handle
//   Sphere   - lamp head dome
//   Box      - stacked books (x3)
// ---------------------------------------------------------------------------
void SceneManager::PrepareScene()
{
    // Load and bind textures into GPU texture slots
    LoadSceneTextures();

    // Register Phong material definitions for all surface types
    DefineObjectMaterials();

    // Upload lighting uniforms to the fragment shader
    SetupSceneLights();

    // Load mesh geometry into GPU vertex buffers (each type loaded once)
    m_basicMeshes->LoadPlaneMesh();
    m_basicMeshes->LoadCylinderMesh();
    m_basicMeshes->LoadTorusMesh();
    m_basicMeshes->LoadSphereMesh();
    m_basicMeshes->LoadBoxMesh();
}

// ---------------------------------------------------------------------------
// RenderScene()
//
// Called every frame.  Transforms, materials, textures, and draw calls for
// every object in the scene.
//
// Objects:
//   1. Desk surface     - large plane; wood texture; matte Phong material
//   2. Lamp base        - wide short cylinder; dark metal texture + material
//   3. Lower arm        - tall thin cylinder; brushed metal texture + material
//   4. Pivot joint      - torus ring;  dark metal texture + material
//   5. Upper arm        - angled cylinder; brushed metal texture + material
//   6. Lamp head        - sphere; gold texture + material
//   7. Coffee mug        - cylinder body + torus handle; ceramic texture
//   8-10. Stack of books - three boxes; book cover texture + material
//
// Per-object rendering sequence (same for every shape):
//   SetTransformations() -> SetShaderTexture() -> SetTextureUVScale()
//                        -> SetShaderMaterial() -> Draw()
// ---------------------------------------------------------------------------
void SceneManager::RenderScene()
{
    // Reusable transform variables
    glm::vec3 scaleXYZ;
    float XrotationDegrees = 0.0f;
    float YrotationDegrees = 0.0f;
    float ZrotationDegrees = 0.0f;
    glm::vec3 positionXYZ;

    // =========================================================================
    // OBJECT 1 - DESK / TABLE SURFACE  (rubric criterion 1: 3-D base plane)
    // =========================================================================
    // Large flat plane representing the desk.  The wood texture is tiled 4x4
    // to avoid stretching across the 20-unit wide surface.
    //
    // Rubric: "Apply shaders that reflect light off a plane."
    // The wood material has a moderate specular component and shininess = 16
    // so the directional overhead light produces a visible soft sheen across
    // the desk top that shifts as the camera moves around it.
    // =========================================================================
    scaleXYZ = glm::vec3(20.0f, 1.0f, 10.0f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 0.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    // Wood texture - tiled 4x4 to fill the wide surface without stretching
    SetShaderTexture("wood");
    SetTextureUVScale(4.0f, 4.0f);

    // Wood Phong material: moderate diffuse, faint specular -> soft sheen
    SetShaderMaterial("wood");

    m_basicMeshes->DrawPlaneMesh();

    // =========================================================================
    // OBJECT 2 - LAMP BASE
    // =========================================================================
    // Wide, low cylinder anchoring the lamp to the desk.
    // Dark metal material: high shininess produces tight specular highlight.
    //
    // Rubric: "Apply shaders that display the texture of a complex object."
    // Dark metal texture + dark_metal Phong material applied here.
    // =========================================================================
    scaleXYZ = glm::vec3(1.5f, 0.25f, 1.5f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 0.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("dark_metal");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("dark_metal");

    m_basicMeshes->DrawCylinderMesh();

    // =========================================================================
    // OBJECT 3 - LOWER ARM
    // =========================================================================
    // Tall thin cylinder rising from the base center up to the pivot joint.
    // Brushed-metal texture tiled 1x3 along the height to avoid stretching.
    // Shininess = 64 gives a broader highlight than polished metal.
    // =========================================================================
    scaleXYZ = glm::vec3(0.15f, 3.0f, 0.15f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 0.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(0.0f, 0.25f, 0.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("brushed_metal");
    SetTextureUVScale(1.0f, 3.0f);
    SetShaderMaterial("brushed_metal");

    m_basicMeshes->DrawCylinderMesh();

    // =========================================================================
    // OBJECT 4 - PIVOT JOINT (TORUS)
    // =========================================================================
    // Small torus ring at the elbow between the two arm sections.
    // Rotated 90 degrees in X so the ring faces the viewer from the front.
    // Matches the lamp base material (dark metal) for visual coherence.
    // =========================================================================
    scaleXYZ = glm::vec3(0.25f, 0.25f, 0.25f);
    XrotationDegrees = 90.0f;
    YrotationDegrees = 0.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(0.0f, 3.25f, 0.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("dark_metal");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("dark_metal");

    m_basicMeshes->DrawTorusMesh();

    // =========================================================================
    // OBJECT 5 - UPPER ARM
    // =========================================================================
    // Shorter cylinder angled -30 degrees in Z from the pivot joint.
    // Same brushed-metal material as the lower arm for visual consistency.
    //
    // Tip position math (arm length = 2.0, tilt = 30 deg from vertical):
    //   X offset = sin(30) * 2.0 = 1.0
    //   Y offset = cos(30) * 2.0 = 1.73
    // =========================================================================
    scaleXYZ = glm::vec3(0.12f, 2.0f, 0.12f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 0.0f;
    ZrotationDegrees = -30.0f;
    positionXYZ = glm::vec3(0.0f, 3.25f, 0.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("brushed_metal");
    SetTextureUVScale(1.0f, 2.0f);
    SetShaderMaterial("brushed_metal");

    m_basicMeshes->DrawCylinderMesh();

    // =========================================================================
    // OBJECT 6 - LAMP HEAD (SPHERE)
    // =========================================================================
    // Rounded dome at the tip of the upper arm.  The gold material has a
    // warm golden specular tint and shininess = 96 so it catches light
    // prominently and acts as the visual focal point of the scene.
    //
    // Position = pivot (0, 3.25) + arm tip offset (1.0, 1.73) = (1.0, 4.98)
    // =========================================================================
    scaleXYZ = glm::vec3(0.55f, 0.55f, 0.55f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 0.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(1.0f, 4.98f, 0.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("gold");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("gold");

    m_basicMeshes->DrawSphereMesh();

    // =========================================================================
    // OBJECT 7 - COFFEE MUG BODY (CYLINDER)
    // =========================================================================
    // Compound object (1 of 2 required basic shapes): the mug body is a short
    // wide cylinder, the handle below is a torus. Positioned to the right of
    // the lamp on the desk surface, matching the reference photo layout.
    // =========================================================================
    scaleXYZ = glm::vec3(0.55f, 0.5f, 0.55f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 0.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(3.0f, 0.5f, 1.5f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("ceramic");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("ceramic");

    m_basicMeshes->DrawCylinderMesh();

    // =========================================================================
    // OBJECT 7b - COFFEE MUG HANDLE (TORUS)
    // =========================================================================
    // Torus rotated 90 degrees about Y so the ring faces outward from the
    // side of the mug body, then offset along X to sit flush against it.
    // Shares the ceramic texture/material so it reads as one continuous mug.
    // =========================================================================
    scaleXYZ = glm::vec3(0.18f, 0.18f, 0.18f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 90.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(3.55f, 0.65f, 1.5f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("ceramic");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("ceramic");

    m_basicMeshes->DrawTorusMesh();

    // =========================================================================
    // OBJECT 8, 9, 10 - STACK OF BOOKS (BOX x3)
    // =========================================================================
    // Three flat, elongated boxes stacked with slight rotational offsets to
    // mimic an unevenly stacked pile, matching the reference photo. Each box
    // is wider/longer than it is tall to read clearly as a book.
    // =========================================================================

    // Bottom book - largest, slight rotation for a casual stacked look
    scaleXYZ = glm::vec3(2.0f, 0.25f, 1.4f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = -4.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(-3.0f, 0.125f, 2.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("book_cover");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("book_cover");

    m_basicMeshes->DrawBoxMesh();

    // Middle book - slightly smaller, opposite rotation offset
    scaleXYZ = glm::vec3(1.8f, 0.22f, 1.25f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = 5.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(-3.0f, 0.36f, 2.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("book_cover");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("book_cover");

    m_basicMeshes->DrawBoxMesh();

    // Top book - smallest, near-aligned on top of the stack
    scaleXYZ = glm::vec3(1.6f, 0.2f, 1.1f);
    XrotationDegrees = 0.0f;
    YrotationDegrees = -2.0f;
    ZrotationDegrees = 0.0f;
    positionXYZ = glm::vec3(-3.0f, 0.565f, 2.0f);

    SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees,
        ZrotationDegrees, positionXYZ);

    SetShaderTexture("book_cover");
    SetTextureUVScale(1.0f, 1.0f);
    SetShaderMaterial("book_cover");

    m_basicMeshes->DrawBoxMesh();
}