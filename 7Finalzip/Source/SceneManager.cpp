///////////////////////////////////////////////////////////////////////////////
// shadermanager.cpp
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// I always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	unsigned char* image = stbi_load(filename, &width, &height, &colorChannels, 0);

	if (image)
	{
		std::cout << "Successfully loaded image:" << filename
			<< ", width:" << width << ", height:" << height
			<< ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			stbi_image_free(image);
			return false;
		}

		glGenerateMipmap(GL_TEXTURE_2D);

		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // unbind the texture

		// register texture and associate it with tag
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method binds the loaded textures to OpenGL memory slots
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method frees memory for all loaded textures.
 *  (Fixed: use glDeleteTextures instead of glGenTextures)
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glDeleteTextures(1, &m_textureIDs[i].ID);
	}
	m_loadedTextures = 0;
}

/***********************************************************
 *  FindTextureID()
 *
 *  Get the OpenGL texture ID for the tag
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		if (m_textureIDs[i].tag == tag)
			return m_textureIDs[i].ID;
	}
	return -1;
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  Get the texture unit index for the tag
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		if (m_textureIDs[i].tag == tag)
			return i;
	}
	return -1;
}

/***********************************************************
 *  FindMaterial()
 *
 *  Get a material associated with a tag
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.empty())
		return false;

	for (auto& mat : m_objectMaterials)
	{
		if (mat.tag == tag)
		{
			material = mat;
			return true;
		}
	}
	return false;
}

/***********************************************************
 *  SetTransformations()
 *
 *  Set transform buffer using scale, rotation, translation
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	glm::mat4 modelView = glm::translate(positionXYZ) *
		glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1, 0, 0)) *
		glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0, 1, 0)) *
		glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0, 0, 1)) *
		glm::scale(scaleXYZ);

	if (m_pShaderManager)
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
}

/***********************************************************
 *  SetShaderColor()
 *
 *  Set the color into the shader for the next draw
 ***********************************************************/
void SceneManager::SetShaderColor(float r, float g, float b, float a)
{
	glm::vec4 color(r, g, b, a);

	if (m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, color);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  Set the texture for the shader
 ***********************************************************/
void SceneManager::SetShaderTexture(std::string textureTag)
{
	if (m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);
		int slot = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, slot);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  Set the UV scale for the shader
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (m_pShaderManager)
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  Pass material values into the shader
 ***********************************************************/
void SceneManager::SetShaderMaterial(std::string materialTag)
{
	if (!m_objectMaterials.empty())
	{
		OBJECT_MATERIAL material;
		if (FindMaterial(materialTag, material))
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/***********************************************************
// STUDENTS CAN MODIFY BELOW METHODS FOR SCENE RENDERING
***********************************************************/

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{


	{
		OBJECT_MATERIAL goldMaterial;
		goldMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.1f);
		goldMaterial.ambientStrength = 0.4f;
		goldMaterial.diffuseColor = glm::vec3(0.3f, 0.3f, 0.2f);
		goldMaterial.specularColor = glm::vec3(0.6f, 0.5f, 0.4f);
		goldMaterial.shininess = 22.0;
		goldMaterial.tag = "gold";
		m_objectMaterials.push_back(goldMaterial);

		OBJECT_MATERIAL cementMaterial;
		cementMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.2f);
		cementMaterial.ambientStrength = 0.2f;
		cementMaterial.diffuseColor = glm::vec3(0.5f, 0.5f, 0.5f);
		cementMaterial.specularColor = glm::vec3(0.4f, 0.4f, 0.4f);
		cementMaterial.shininess = 0.5;
		cementMaterial.tag = "cement";
		m_objectMaterials.push_back(cementMaterial);

		OBJECT_MATERIAL woodMaterial;
		woodMaterial.ambientColor = glm::vec3(0.4f, 0.3f, 0.1f);
		woodMaterial.ambientStrength = 0.2f;
		woodMaterial.diffuseColor = glm::vec3(0.3f, 0.2f, 0.1f);
		woodMaterial.specularColor = glm::vec3(0.1f, 0.1f, 0.1f);
		woodMaterial.shininess = 0.3;
		woodMaterial.tag = "wood";
		m_objectMaterials.push_back(woodMaterial);

		OBJECT_MATERIAL tileMaterial;
		tileMaterial.ambientColor = glm::vec3(0.2f, 0.3f, 0.4f);
		tileMaterial.ambientStrength = 0.3f;
		tileMaterial.diffuseColor = glm::vec3(0.3f, 0.2f, 0.1f);
		tileMaterial.specularColor = glm::vec3(0.4f, 0.5f, 0.6f);
		tileMaterial.shininess = 25.0;
		tileMaterial.tag = "tile";
		m_objectMaterials.push_back(tileMaterial);

		OBJECT_MATERIAL glassMaterial;
		glassMaterial.ambientColor = glm::vec3(0.4f, 0.4f, 0.4f);
		glassMaterial.ambientStrength = 0.3f;
		glassMaterial.diffuseColor = glm::vec3(0.3f, 0.3f, 0.3f);
		glassMaterial.specularColor = glm::vec3(0.6f, 0.6f, 0.6f);
		glassMaterial.shininess = 85.0;
		glassMaterial.tag = "glass";
		m_objectMaterials.push_back(glassMaterial);

		OBJECT_MATERIAL clayMaterial;
		clayMaterial.ambientColor = glm::vec3(0.2f, 0.2f, 0.3f);
		clayMaterial.ambientStrength = 0.3f;
		clayMaterial.diffuseColor = glm::vec3(0.4f, 0.4f, 0.5f);
		clayMaterial.specularColor = glm::vec3(0.2f, 0.2f, 0.4f);
		clayMaterial.shininess = 0.5;
		clayMaterial.tag = "clay";
		m_objectMaterials.push_back(clayMaterial);
	}


	

	{
		// Light 0
		m_pShaderManager->setVec3Value("lightSources[0].position", 3.0f, 14.0f, 0.0f);
		m_pShaderManager->setVec3Value("lightSources[0].ambientColor", 0.1f, 0.1f, 0.1f);  
		m_pShaderManager->setVec3Value("lightSources[0].diffuseColor", 0.6f, 0.6f, 0.6f);  

		m_pShaderManager->setVec3Value("lightSources[0].specularColor", 0.0f, 0.0f, 0.0f);
		m_pShaderManager->setFloatValue("lightSources[0].focalStrength", 32.0f);
		m_pShaderManager->setFloatValue("lightSources[0].specularIntensity", 0.05f);

		// Light 1
		m_pShaderManager->setVec3Value("lightSources[1].position", -3.0f, 14.0f, 0.0f);
		m_pShaderManager->setVec3Value("lightSources[1].ambientColor", 0.1f, 0.1f, 0.1f);
		m_pShaderManager->setVec3Value("lightSources[1].diffuseColor", 0.6f, 0.6f, 0.6f);
		m_pShaderManager->setVec3Value("lightSources[1].specularColor", 0.0f, 0.0f, 0.0f);
		m_pShaderManager->setFloatValue("lightSources[1].focalStrength", 32.0f);
		m_pShaderManager->setFloatValue("lightSources[1].specularIntensity", 0.05f);

		// Light 2
		m_pShaderManager->setVec3Value("lightSources[2].position", 0.6f, 5.0f, 6.0f);
		m_pShaderManager->setVec3Value("lightSources[2].ambientColor", 0.1f, 0.1f, 0.1f);
		m_pShaderManager->setVec3Value("lightSources[2].diffuseColor", 0.6f, 0.6f, 0.6f);
		m_pShaderManager->setVec3Value("lightSources[2].specularColor", 0.3f, 0.3f, 0.3f);
		m_pShaderManager->setFloatValue("lightSources[2].focalStrength", 12.0f);
		m_pShaderManager->setFloatValue("lightSources[2].specularIntensity", 0.5f);

		// Enable lighting
		m_pShaderManager->setBoolValue("bUseLighting", true);
	}

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadConeMesh();   // for all cones
	m_basicMeshes->LoadTorusMesh();  // for torus around center cone
	m_basicMeshes->LoadBoxMesh();    // for the box on left

	// ===========================
	// Load textures into memory
	// ===========================
	bool bReturn = false;

	// Floor plane texture
	bReturn = CreateGLTexture("../../Utilities/textures/brick.jpg", "floor");

	// Cones texture
	bReturn = CreateGLTexture("../../Utilities/textures/breadcrust.jpg", "cone");

	// Box texture
	bReturn = CreateGLTexture("../../Utilities/textures/gold-seamless-texture.jpg", "box");

	// After loading textures, bind them to OpenGL slots
	BindGLTextures();
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	glm::mat4 scale;
	glm::mat4 rotation;
	glm::mat4 rotation2;
	glm::mat4 translation;
	glm::mat4 model;

	/******************************************************************/
	/*** PLANE — floor with brick texture ***/
	scaleXYZ = glm::vec3(20.0f, 1.0f, 20.0f);
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);
	SetTransformations(scaleXYZ, XrotationDegrees, YrotationDegrees, ZrotationDegrees, positionXYZ);

	SetShaderTexture("floor");       // apply brick texture to plane
	SetTextureUVScale(4.0f, 4.0f);   // repeat texture 4x in U and V
	m_basicMeshes->DrawPlaneMesh();

	/******************************************************************/
	/*** CENTER CONE — abstract texture ***/
	scaleXYZ = glm::vec3(1.0f, 2.0f, 1.0f);
	positionXYZ = glm::vec3(0.0f, 1.0f, 3.0f); // centered
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("cone"); // abstract texture
	m_basicMeshes->DrawConeMesh();

	/*** Torus around center cone — abstract texture ***/
	scaleXYZ = glm::vec3(1.6f, 1.6f, 1.6f);
	positionXYZ = glm::vec3(0.0f, 1.0f, 3.0f);
	SetTransformations(scaleXYZ, 90.0f, 0, 0, positionXYZ);
	SetShaderTexture("cone"); // same abstract texture for torus
	m_basicMeshes->DrawTorusMesh();

	/******************************************************************/
	/*** FRONT ROW — LARGE CONES (LEFT & RIGHT) — abstract texture ***/
	scaleXYZ = glm::vec3(1.6f, 2.0f, 1.6f);

	// Left
	positionXYZ = glm::vec3(-6.0f, 0.5f, 8.0f);
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("cone");
	m_basicMeshes->DrawConeMesh();

	// Right
	positionXYZ = glm::vec3(6.0f, 0.5f, 8.0f);
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("cone");
	m_basicMeshes->DrawConeMesh();

	/******************************************************************/
	/*** SECOND ROW — MEDIUM CONES (LEFT & RIGHT) — abstract texture ***/
	scaleXYZ = glm::vec3(1.2f, 2.0f, 1.2f);

	// Left
	positionXYZ = glm::vec3(-4.0f, 0.5f, 5.0f);
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("cone");
	m_basicMeshes->DrawConeMesh();

	// Right
	positionXYZ = glm::vec3(4.0f, 0.5f, 5.0f);
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("cone");
	m_basicMeshes->DrawConeMesh();

	/******************************************************************/
	/*** THIRD ROW — SMALL CONES (LEFT & RIGHT) — abstract texture ***/
	scaleXYZ = glm::vec3(0.9f, 2.0f, 0.9f);

	// Left
	positionXYZ = glm::vec3(-2.0f, 0.5f, -3.0f);
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("cone");
	m_basicMeshes->DrawConeMesh();

	// Right
	positionXYZ = glm::vec3(2.0f, 0.5f, -3.0f);
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("cone");
	m_basicMeshes->DrawConeMesh();

	/******************************************************************/
	/*** BOX — gold-seamless-texture ***/
	scaleXYZ = glm::vec3(0.3f, 2.0f, 3.5f);
	positionXYZ = glm::vec3(-5.0f, 0.6f, 6.5f);
	SetTransformations(scaleXYZ, 0, 0, 0, positionXYZ);
	SetShaderTexture("box");
	m_basicMeshes->DrawBoxMesh();
}

