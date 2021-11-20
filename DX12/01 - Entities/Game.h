#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Transform.h"
#include "Camera.h"

#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <vector>
#include <memory>

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

	bool vsync;

	// Initialization helper methods - feel free to customize, combine, etc.
	void CreateRootSigAndPipelineState();
	void CreateBasicGeometry();
	
	// Overall pipeline and rendering requirements
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	// Scene
	std::shared_ptr<Camera> camera;
	std::vector<std::shared_ptr<GameEntity>> entities;

};

