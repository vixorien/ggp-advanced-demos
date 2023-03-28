#include "Scene.h"
#include "Assets.h"
#include "Helpers.h"

#include <fstream>
#include "../../Common/json/json.hpp"
using json = nlohmann::json;

using namespace DirectX;

Scene::Scene(
	std::string name,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) 
	:
	name(name),
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

void Scene::AddLight(Light light) 
{ 
	lights.push_back(light); 
}

void Scene::SetName(std::string name) { this->name = name; }
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
std::string Scene::GetName() { return name; }
std::shared_ptr<Sky> Scene::GetSky() { return sky; }
std::shared_ptr<Camera> Scene::GetCurrentCamera() { return currentCamera; }


std::shared_ptr<Scene> Scene::Load(
	std::wstring sceneFile,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Open and parse
	std::ifstream file(sceneFile);
	json sceneJson = json::parse(file);
	file.close();

	// Check for name
	std::string name = "Scene";
	if (sceneJson.contains("name"))
	{
		name = sceneJson["name"].get<std::string>();
	}

	// Create the scene
	std::shared_ptr<Scene> scene = std::make_shared<Scene>(name, device, context);

	// Check for sky
	if (sceneJson.contains("sky") && sceneJson["sky"].is_object())
	{
		scene->SetSky(Sky::Parse(sceneJson["sky"], device, context));
	}

	// Check for cameras
	if (sceneJson.contains("cameras") && sceneJson["cameras"].is_array())
	{
		for (int c = 0; c < sceneJson["cameras"].size(); c++)
			scene->AddCamera(Camera::Parse(sceneJson["cameras"][c]));
	}

	// Create a default camera if none were loaded
	if(scene->GetCameras().size() == 0)
	{
		scene->AddCamera(std::make_shared<Camera>(0.0f, 0.0f, -5.0f, 5.0f, 0.001f, XM_PIDIV4, 1.0f));
	}

	// Check for lights
	if (sceneJson.contains("lights") && sceneJson["lights"].is_array())
	{
		for (int l = 0; l < sceneJson["lights"].size(); l++)
			scene->AddLight(Light::Parse(sceneJson["lights"][l]));
	}

	// Check for entities
	if (sceneJson.contains("entities") && sceneJson["entities"].is_array())
	{
		for (int e = 0; e < sceneJson["entities"].size(); e++)
			scene->AddEntity(GameEntity::Parse(sceneJson["entities"][e]));
	}

	return scene;
}
