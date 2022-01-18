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

	// Our scene
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::shared_ptr<Camera> camera;

	// Lights
	std::vector<Light> lights;
	int lightCount;

	// These will be loaded along with other assets and
	// saved to these variables for ease of access
	std::shared_ptr<Mesh> lightMesh;
	std::shared_ptr<SimpleVertexShader> lightVS;
	std::shared_ptr<SimplePixelShader> lightPS;

	// Texture related resources
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;

	// Skybox
	std::shared_ptr<Sky> sky;

	// IMGUI-related methods
	void CreateUI(float dt);
	void UILight(Light& light, int index);

	// General helpers for setup and drawing
	void GenerateLights();
	void DrawPointLights();

	// Initialization helper method
	void LoadAssetsAndCreateEntities();
};

