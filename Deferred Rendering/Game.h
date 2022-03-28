#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"
#include "Renderer.h"
#include "ImGui/imgui.h"

#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <vector>

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);

private:

	// Keep track of "stuff" to clean up
	std::vector<Material*> materials;
	std::vector<GameEntity*> entities;
	Camera* camera;

	// Smart renderer
	Renderer* renderer;

	// Lights
	std::vector<Light> lights;

	// Texture related resources
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;

	// Skybox
	Sky* sky;

	// IMGUI-related methods
	void CreateUI(float dt);
	void UIEntity(GameEntity* entity, int index);
	void UILight(Light& light, int index);
	void ImageWithHover(ImTextureID user_texture_id, const ImVec2& size);
	void ImageWithHover(ImTextureID user_texture_id, const ImVec2& size, const char* name);

	// General helpers
	void GenerateLights();
	void LoadAssetsAndCreateEntities();
};

