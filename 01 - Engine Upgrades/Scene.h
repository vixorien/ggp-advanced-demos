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
	void SetCurrentCamera(std::shared_ptr<Camera> camera);
	void SetCurrentCamera(unsigned int cameraIndex);

	std::vector<Light>& GetLights();
	std::vector<std::shared_ptr<Camera>>& GetCameras();
	std::vector<std::shared_ptr<GameEntity>>& GetEntities();

	std::shared_ptr<Sky> GetSky();
	std::shared_ptr<Camera> GetCurrentCamera();

private:

	// Drawing and setup requirements
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

	// Vectors of various scene elements
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::vector<std::shared_ptr<Camera>> cameras;
	std::vector<Light> lights;

	// Singular elements
	std::shared_ptr<Camera> currentCamera;
	std::shared_ptr<Sky> sky;
};

