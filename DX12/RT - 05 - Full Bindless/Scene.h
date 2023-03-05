#pragma once

#include "GameEntity.h"

#include <wrl/client.h>
#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>

class Scene
{
public:

	Scene(std::string name);
	~Scene();

	std::string GetName(); 
	unsigned int EntityCount();
	std::vector<std::shared_ptr<GameEntity>> GetEntities();

	void AddEntity(std::shared_ptr<GameEntity> entity);
	std::shared_ptr<GameEntity> GetEntity(unsigned int index);

	static void UpdateScene(std::shared_ptr<Scene> scene, float deltaTime, float totalTime);
	static std::vector<std::shared_ptr<Scene>> CreateExampleScenes(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState);

private:
	std::string name; 
	std::vector<std::shared_ptr<GameEntity>> entities;

	static bool exampleScenesCreated;
	static std::vector<std::shared_ptr<Scene>> exampleScenes;
};

