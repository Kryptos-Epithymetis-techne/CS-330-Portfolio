///////////////////////////////////////////////////////////////////////////////
// SceneManager.h
// ==============
// Manages the preparing and rendering of 3D scenes.
// Handles textures, Phong materials, lighting configuration,
// shader uniform uploads, and object rendering.
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  MODIFIED BY: Student - CS-330 8-1 Final Project (lighting milestone)
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ShaderManager.h"
#include "ShapeMeshes.h"
#include <string>
#include <vector>

/***********************************************************
 *  SceneManager
 *
 *  Prepares and renders the 3D scene, including textures,
 *  Phong materials, and lighting setup.
 ***********************************************************/
class SceneManager
{
public:
	// Constructor / Destructor
	SceneManager(ShaderManager* pShaderManager);
	~SceneManager();

	// Texture registry entry: OpenGL texture ID paired with a lookup tag
	struct TEXTURE_INFO
	{
		std::string tag;
		uint32_t ID = 0;
	};

	// Phong material: diffuse color, specular color, shininess exponent, tag
	struct OBJECT_MATERIAL
	{
		glm::vec3 diffuseColor = glm::vec3(0.5f);
		glm::vec3 specularColor = glm::vec3(0.5f);
		float     shininess = 32.0f;
		std::string tag;
	};

private:
	// Pointer to the shared shader manager (not owned by this class)
	ShaderManager* m_pShaderManager;

	// Pointer to the basic shape mesh library
	ShapeMeshes* m_basicMeshes;

	// Number of textures currently loaded into m_textureIDs
	int m_loadedTextures = 0;

	// Texture registry - supports up to 16 simultaneous textures
	TEXTURE_INFO m_textureIDs[16];

	// Material registry - all defined Phong materials for the scene
	std::vector<OBJECT_MATERIAL> m_objectMaterials;

	// -----------------------------------------------------------------------
	// Texture management
	// -----------------------------------------------------------------------

	// This will Load an image file into an OpenGL texture and register it under 'tag'
	bool CreateGLTexture(const char* filename, std::string tag);

	// This bad boy Binds all loaded textures to GL_TEXTURE0 .. GL_TEXTURE(n) slots
	void BindGLTextures();

	// This Releases all GPU texture memory
	void DestroyGLTextures();

	// This Returns the OpenGL texture object ID for the given tag (-1 if not found)
	int FindTextureID(std::string tag);

	// This Returns the texture unit slot index for the given tag (-1 if not found)
	int FindTextureSlot(std::string tag);

	// -----------------------------------------------------------------------
	// Material management
	// -----------------------------------------------------------------------

	// Search the material list for 'tag'; copy result into 'material'
	bool FindMaterial(std::string tag, OBJECT_MATERIAL& material);

	// -----------------------------------------------------------------------
	// Shader uniform helpers
	// -----------------------------------------------------------------------

	// Build and upload the model matrix (Scale->Rotate->Translate)
	void SetTransformations(
		glm::vec3 scaleXYZ,
		float XrotationDegrees,
		float YrotationDegrees,
		float ZrotationDegrees,
		glm::vec3 positionXYZ);

	// Upload a flat RGBA color and disable texture sampling
	void SetShaderColor(
		float redColorValue,
		float greenColorValue,
		float blueColorValue,
		float alphaValue);

	// Enable texture sampling and upload the slot for the given tag
	void SetShaderTexture(std::string textureTag);

	// Upload the UV tiling scale for the next draw call
	void SetTextureUVScale(float u, float v);

	// Look up the named material and upload its Phong properties to the shader
	void SetShaderMaterial(std::string materialTag);

	// -----------------------------------------------------------------------
	// Private scene-setup helpers (called once from PrepareScene)
	// -----------------------------------------------------------------------

	// Load all texture image files and bind them to GPU texture slots
	void LoadSceneTextures();

	// Populate m_objectMaterials with Phong material definitions for each
	// surface type in the scene (wood, dark metal, brushed metal, gold)
	void DefineObjectMaterials();

	// Upload all light source uniforms to the fragment shader and enable
	// the Phong lighting path (bUseLighting = true)
	void SetupSceneLights();

public:
	// -----------------------------------------------------------------------
	// Public scene interface (called from main render loop)
	// -----------------------------------------------------------------------

	// Load meshes, textures, materials, and lighting - called once at startup
	void PrepareScene();

	// Transform and draw all objects - called every frame
	void RenderScene();
};