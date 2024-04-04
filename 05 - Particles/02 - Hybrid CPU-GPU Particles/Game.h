#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"
#include "ImGui/imgui.h"
#include "Emitter.h"

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

	std::vector<std::shared_ptr<GameEntity>> entities;
	std::shared_ptr<Camera> camera;

	// Lights
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;
	int lightCount;

	// Particles
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> particleDepthState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> particleBlendState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> particleDebugRasterState;
	std::vector<std::shared_ptr<Emitter>> emitters;
	void DrawParticles(float totalTime);

	// Skybox
	std::shared_ptr<Sky> sky;

	// IMGUI-related methods
	void CreateUI(float dt);
	void UIEmitter(std::shared_ptr<Emitter> emitter, int index);

	// General helpers
	void GenerateLights();
	void LoadAssetsAndCreateEntities();
};

