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
#include "../../Common/ImGui/imgui.h"

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

	// Our scene
	std::shared_ptr<Scene> scene;

	// Rendering
	std::shared_ptr<Renderer> renderer;

	// General helpers for setup and drawing
	void LoadAssetsAndCreateEntities();
	void GenerateLights();
	void AddRandomEntity();

	// UI functions
	void UINewFrame(float deltaTime);
	void BuildUI();
	void CameraUI(std::shared_ptr<Camera> cam);
	void EntityUI(std::shared_ptr<GameEntity> entity);	
	void LightUI(Light& light);
	void ImageWithHover(ImTextureID user_texture_id, const ImVec2& size);
	
	// General options, controlled through UI
	bool useOptimizedRendering;
	bool showUIDemoWindow;
	int lightCount;
};

