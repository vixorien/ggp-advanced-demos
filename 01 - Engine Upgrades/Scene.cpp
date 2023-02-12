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

void Scene::AddLight(Light light)
{
	lights.push_back(light);
}

void Scene::SetSky(std::shared_ptr<Sky> sky)
{
	this->sky = sky;
}

void Scene::Update(float deltaTime)
{
	if (currentCamera)
		currentCamera->Update(deltaTime);
}

void Scene::Draw()
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
		sky = ParseSky(scene["sky"]);
	}

	// Check for cameras
	if (scene.contains("cameras") && scene["cameras"].is_array())
	{
		for (int c = 0; c < scene["cameras"].size(); c++)
			AddCamera(ParseCamera(scene["cameras"][c]));
	}

	// Check for lights
	if (scene.contains("lights") && scene["lights"].is_array())
	{
		for (int l = 0; l < scene["lights"].size(); l++)
			AddLight(ParseLight(scene["lights"][l]));
	}

	// Check for entities
	if (scene.contains("entities") && scene["entities"].is_array())
	{
		for (int e = 0; e < scene["entities"].size(); e++)
			AddEntity(ParseEntity(scene["entities"][e]));
	}
}

std::shared_ptr<Sky> Scene::ParseSky(json j)
{
	Assets& assets = Assets::GetInstance();

	json t = j["texture"];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> xPos = assets.GetTexture(NarrowToWide(t["xPos"].get<std::string>()));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> xNeg = assets.GetTexture(NarrowToWide(t["xNeg"].get<std::string>()));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> yPos = assets.GetTexture(NarrowToWide(t["yPos"].get<std::string>()));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> yNeg = assets.GetTexture(NarrowToWide(t["yNeg"].get<std::string>()));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> zPos = assets.GetTexture(NarrowToWide(t["zPos"].get<std::string>()));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> zNeg = assets.GetTexture(NarrowToWide(t["zNeg"].get<std::string>()));
	std::shared_ptr<Mesh> mesh = assets.GetMesh(NarrowToWide(j["mesh"].get<std::string>()));
	std::shared_ptr<SimpleVertexShader> vs = assets.GetVertexShader(NarrowToWide(j["shaders"]["vertex"].get<std::string>()));
	std::shared_ptr<SimplePixelShader> ps = assets.GetPixelShader(NarrowToWide(j["shaders"]["pixel"].get<std::string>()));
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler = assets.GetSampler(NarrowToWide(j["sampler"].get<std::string>()));

	return std::make_shared<Sky>(
		xPos, xNeg, yPos, yNeg, zPos, zNeg,
		mesh,
		vs, ps,
		sampler,
		device,
		context);
}

std::shared_ptr<Camera> Scene::ParseCamera(json j)
{
	// Defaults
	CameraProjectionType projType = CameraProjectionType::Perspective;
	float moveSpeed = 5.0f;
	float lookSpeed = 0.002f;
	float fov = XM_PIDIV4;
	float nearClip = 0.01f;
	float farClip = 1000.0f;
	XMFLOAT3 pos = { 0,0,-5 };
	XMFLOAT3 rot = { 0, 0, 0 };

	// Check for each type of data
	if (j.contains("type") && j["type"].get<std::string>() == "orthographic")
		projType = CameraProjectionType::Orthographic;

	if (j.contains("moveSpeed")) moveSpeed = j["moveSpeed"].get<float>();
	if (j.contains("lookSpeed")) lookSpeed = j["lookSpeed"].get<float>();
	if (j.contains("fov")) fov = j["fov"].get<float>();
	if (j.contains("near")) nearClip = j["near"].get<float>();
	if (j.contains("far")) farClip = j["far"].get<float>();
	if (j.contains("position") && j["position"].size() == 3)
	{
		pos.x = j["position"][0].get<float>();
		pos.y = j["position"][1].get<float>();
		pos.z = j["position"][2].get<float>();
	}
	if (j.contains("rotation") && j["rotation"].size() == 3)
	{
		rot.x = j["rotation"][0].get<float>();
		rot.y = j["rotation"][1].get<float>();
		rot.z = j["rotation"][2].get<float>();
	}

	// Create the camera
	std::shared_ptr<Camera> cam = std::make_shared<Camera>(
		pos, moveSpeed, lookSpeed, fov, 1.0f, nearClip, farClip, projType);
	cam->GetTransform()->SetRotation(rot);

	return cam;
}

std::shared_ptr<GameEntity> Scene::ParseEntity(json j)
{
	Assets& assets = Assets::GetInstance();

	std::shared_ptr<GameEntity> entity = std::make_shared<GameEntity>(
		assets.GetMesh(NarrowToWide(j["mesh"].get<std::string>())),
		assets.GetMaterial(NarrowToWide(j["material"].get<std::string>())));

	// Early out if transform is missing
	if (!j.contains("transform")) return entity;
	json tr = j["transform"];

	// Handle transform
	XMFLOAT3 pos = { 0, 0, 0};
	XMFLOAT3 rot = { 0, 0, 0 };
	XMFLOAT3 sc = { 1, 1, 1 };

	if (tr.contains("position") && tr["position"].size() == 3)
	{
		pos.x = tr["position"][0].get<float>();
		pos.y = tr["position"][1].get<float>();
		pos.z = tr["position"][2].get<float>();
	}

	if (tr.contains("rotation") && tr["rotation"].size() == 3)
	{
		rot.x = tr["rotation"][0].get<float>();
		rot.y = tr["rotation"][1].get<float>();
		rot.z = tr["rotation"][2].get<float>();
	}

	if (tr.contains("scale") && tr["scale"].size() == 3)
	{
		sc.x = tr["scale"][0].get<float>();
		sc.y = tr["scale"][1].get<float>();
		sc.z = tr["scale"][2].get<float>();
	}

	entity->GetTransform()->SetPosition(pos);
	entity->GetTransform()->SetRotation(rot);
	entity->GetTransform()->SetScale(sc);
	return entity;
}

Light Scene::ParseLight(json j)
{
	// Set defaults
	Light light = {};
	
	// Check data
	if (j.contains("type"))
	{
		if (j["type"].get<std::string>() == "directional") light.Type = LIGHT_TYPE_DIRECTIONAL;
		else if (j["type"].get<std::string>() == "point") light.Type = LIGHT_TYPE_POINT;
		else if (j["type"].get<std::string>() == "spot") light.Type = LIGHT_TYPE_SPOT;
	}

	if (j.contains("direction") && j["direction"].size() == 3)
	{
		light.Direction.x = j["direction"][0].get<float>();
		light.Direction.y = j["direction"][1].get<float>();
		light.Direction.z = j["direction"][2].get<float>();
	}

	if (j.contains("position") && j["position"].size() == 3)
	{
		light.Position.x = j["position"][0].get<float>();
		light.Position.y = j["position"][1].get<float>();
		light.Position.z = j["position"][2].get<float>();
	}

	if (j.contains("color") && j["color"].size() == 3)
	{
		light.Color.x = j["color"][0].get<float>();
		light.Color.y = j["color"][1].get<float>();
		light.Color.z = j["color"][2].get<float>();
	}

	if (j.contains("intensity")) light.Intensity = j["intensity"].get<float>();
	if (j.contains("range")) light.Range = j["range"].get<float>();
	if (j.contains("spotFalloff")) light.SpotFalloff = j["spotFalloff"].get<float>();
	
	return light;
}