#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "SimpleShader.h"
#include "SpriteFont.h"
#include "SpriteBatch.h"
#include "Lights.h"
#include "Sky.h"

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

	// Lights
	std::vector<Light> lights;
	int lightCount;

	// Texture related resources
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;

	// Skybox
	Sky* sky;

	// Text & ui
	DirectX::SpriteFont* arial;
	DirectX::SpriteBatch* spriteBatch;

	// IMGUI-related methods
	void CreateUI(float dt);
	void UILight(Light& light, int index);

	// General helpers 
	void GenerateLights();
	void DrawPointLights();
	void DrawUI();
	void LoadAssetsAndCreateEntities();
};

