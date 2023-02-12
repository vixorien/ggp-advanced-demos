#pragma once


#include "GameEntity.h"
#include "Camera.h"
#include "Lights.h"
#include "Sky.h"

#include "..\Common\json\json.hpp"

#include <d3d11.h>
#include <vector>
#include <memory>
#include <wrl/client.h>

class Scene
{
public:

	Scene(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	~Scene();

	void Clear();
	void Load(std::wstring sceneFile);

	void AddEntity(std::shared_ptr<GameEntity> entity);
	void AddCamera(std::shared_ptr<Camera> camera);
	void AddLight(Light light);

	void SetSky(std::shared_ptr<Sky> sky);
	
	void UpdateAspectRatio(float aspectRatio);

	void Update(float deltaTime);
	void Draw();

	std::shared_ptr<Camera> GetCurrentCamera();
	std::vector<std::shared_ptr<GameEntity>> GetEntities();

private:

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

	// Vectors of various scene objects
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::vector<std::shared_ptr<Camera>> cameras;
	std::vector<Light> lights;

	std::shared_ptr<Camera> currentCamera;
	std::shared_ptr<Sky> sky;

	// Helpers for json parsing
	std::shared_ptr<Sky> ParseSky(nlohmann::json j);
	std::shared_ptr<Camera> ParseCamera(nlohmann::json j);
	std::shared_ptr<GameEntity> ParseEntity(nlohmann::json j);
	Light ParseLight(nlohmann::json j);
};

