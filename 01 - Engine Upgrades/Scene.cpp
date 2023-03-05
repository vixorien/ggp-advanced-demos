#include "Scene.h"
#include "Assets.h"
#include "Helpers.h"

#include <fstream>
#include "../Common/json/json.hpp"
using json = nlohmann::json;

using namespace DirectX;

Scene::Scene(
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) 
	:
	device(device),
	context(context)
{

}

Scene::~Scene()
{
}

void Scene::Clear()
{
	// Clean up any resources we have
	lights.clear();
	cameras.clear();
	entities.clear();

	currentCamera.reset();
	sky.reset();
}

void Scene::AddEntity(std::shared_ptr<GameEntity> entity)
{
	entities.push_back(entity);
}

void Scene::AddCamera(std::shared_ptr<Camera> camera)
{
	cameras.push_back(camera);

	// Make this the current camera if we have none
	if (!currentCamera)
		currentCamera = camera;
}

void Scene::AddLight(Light light) { lights.push_back(light); }

void Scene::SetSky(std::shared_ptr<Sky> sky) { this->sky = sky; }


void Scene::SetCurrentCamera(std::shared_ptr<Camera> camera)
{
	if (std::find(cameras.begin(), cameras.end(), camera) == cameras.end())
		return;

	currentCamera = camera;
}

void Scene::SetCurrentCamera(unsigned int cameraIndex)
{
	if (cameraIndex >= cameras.size())
		return;

	currentCamera = cameras[cameraIndex];
}


std::vector<Light>& Scene::GetLights() { return lights; }
std::vector<std::shared_ptr<Camera>>& Scene::GetCameras() { return cameras; }
std::vector<std::shared_ptr<GameEntity>>& Scene::GetEntities() { return entities; }
std::shared_ptr<Sky> Scene::GetSky() { return sky; }
std::shared_ptr<Camera> Scene::GetCurrentCamera() { return currentCamera; }


void Scene::Load(std::wstring sceneFile)
{
	// Clear existing scene
	Clear();

	// Open and parse
	std::ifstream file(sceneFile);
	json scene = json::parse(file);
	file.close();

	// Check for sky
	if (scene.contains("sky") && scene["sky"].is_object())
	{
		sky = Sky::Parse(scene["sky"], device, context);
	}

	// Check for cameras
	if (scene.contains("cameras") && scene["cameras"].is_array())
	{
		for (int c = 0; c < scene["cameras"].size(); c++)
			AddCamera(Camera::Parse(scene["cameras"][c]));
			//AddCamera(ParseCamera(scene["cameras"][c]));
	}

	// Check for lights
	if (scene.contains("lights") && scene["lights"].is_array())
	{
		for (int l = 0; l < scene["lights"].size(); l++)
			AddLight(Light::Parse(scene["lights"][l]));
	}

	// Check for entities
	if (scene.contains("entities") && scene["entities"].is_array())
	{
		for (int e = 0; e < scene["entities"].size(); e++)
			AddEntity(GameEntity::Parse(scene["entities"][e]));
	}
}
