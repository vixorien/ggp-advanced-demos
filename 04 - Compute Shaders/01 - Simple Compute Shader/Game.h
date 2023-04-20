#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"
#include "Scene.h"
#include "Renderer.h"

#include <DirectXMath.h>
#include <wrl/client.h>
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

	// Compute shader related resources
	std::shared_ptr<SimpleComputeShader> noiseCS;
	unsigned int noiseTextureSize;
	int noiseInterations;
	float noisePersistance;
	float noiseScale;
	float noiseOffset;
	bool computeShaderActive;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> computeTextureSRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> computeTextureUAV;
	void CreateComputeShaderTexture();

	// Our scene
	std::shared_ptr<Scene> scene;

	// Rendering
	std::shared_ptr<Renderer> renderer;

	// General helpers for setup and drawing
	void LoadAssetsAndCreateEntities();
	void GenerateLights();

	// UI functions
	void UINewFrame(float deltaTime);
	void BuildUI();
	void CameraUI(std::shared_ptr<Camera> cam);
	void EntityUI(std::shared_ptr<GameEntity> entity);	
	void LightUI(Light& light);
	
	// General options, controlled through UI
	bool useOptimizedRendering;
	bool showUIDemoWindow;
	int lightCount;
};

