#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "Material.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"
#include "Emitter.h"

#include "SpriteBatch.h"
#include "SpriteFont.h"

#include <vector>
#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects

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

	// Initialization helper methods - feel free to customize, combine, etc.
	void LoadAssetsAndCreateEntities();

	// General helpers for setup and drawing
	void GenerateLights();

	// Camera for the 3D scene
	std::shared_ptr<Camera> camera;

	// The sky box
	std::shared_ptr<Sky> sky;

	// Scene
	std::vector<std::shared_ptr<GameEntity>> entities;

	// Lights
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;
	int lightCount;

	// Sprite batch resources
	std::shared_ptr<DirectX::SpriteBatch> spriteBatch;

	// Particles
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> particleDepthState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> particleBlendState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> particleDebugRasterState;
	std::vector<std::shared_ptr<Emitter>> emitters;
	void DrawParticles();
};

