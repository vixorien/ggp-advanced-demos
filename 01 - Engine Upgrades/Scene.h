#pragma once


#include "GameEntity.h"
#include "Camera.h"
#include "Lights.h"
#include "Sky.h"

#include <d3d11.h>
#include <vector>
#include <memory>
#include <wrl/client.h>

class Scene
{
public:

	Scene();
	~Scene();

	void AddEntity(std::shared_ptr<GameEntity> entity);
	void AddCamera(std::shared_ptr<Camera> camera);
	void AddLight(Light light);

	void SetSky(std::shared_ptr<Sky> sky);
	
	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

private:

	// Vectors of various scene objects
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::vector<std::shared_ptr<Camera>> cameras;
	std::vector<Light> lights;

	std::shared_ptr<Camera> currentCamera;
	std::shared_ptr<Sky> sky;
};

