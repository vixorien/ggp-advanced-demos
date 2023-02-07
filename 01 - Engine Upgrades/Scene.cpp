#include "Scene.h"

#undef max
#include "../Common/rapidjson/document.h"
#include "../Common/rapidjson/filereadstream.h"
#include "../Common/rapidjson/istreamwrapper.h"

#include <fstream>

Scene::Scene()
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

void Scene::SetSky(std::shared_ptr<Sky> sky)
{
	this->sky = sky;
}

void Scene::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Ensure we have something to do
	if (!currentCamera)
		return;

	// Draw entities
	for (auto& ge : entities)
	{
		// Set the "per frame" data
		// Note that this should literally be set once PER FRAME, before
		// the draw loop, but we're currently setting it per entity since 
		// we are just using whichever shader the current entity has.  
		// Inefficient!!!
		std::shared_ptr<SimplePixelShader> ps = ge->GetMaterial()->GetPixelShader();
		ps->SetData("lights", (void*)(&lights[0]), sizeof(Light) * (unsigned int)lights.size());
		ps->SetInt("lightCount", (unsigned int)lights.size());
		ps->SetFloat3("cameraPosition", currentCamera->GetTransform()->GetPosition());
		ps->CopyBufferData("perFrame");

		// Draw the entity
		ge->Draw(context, currentCamera);
	}

	// Draw the sky if necessary
	if (sky)
	{
		sky->Draw(currentCamera);
	}
}

void Scene::Load(std::wstring sceneFile)
{
	// Clear existing scene
	Clear();

	// Open the file and parse with rapidjson
	std::ifstream fileIn(sceneFile.c_str());
	rapidjson::IStreamWrapper stream(fileIn);

	rapidjson::Document d;
	rapidjson::ParseResult parseRes = d.ParseStream(stream);

	if (!parseRes)
	{
		wprintf((L"Failed to load or parse scene: " + sceneFile).c_str());
		return;
	}

	// NOTE: Look into replacing rapidjson with https://github.com/nlohmann/json
}