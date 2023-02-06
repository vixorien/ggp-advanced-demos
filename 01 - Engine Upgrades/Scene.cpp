#include "Scene.h"

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::AddEntity(std::shared_ptr<GameEntity> entity)
{
	entities.push_back(entity);
}

void Scene::AddCamera(std::shared_ptr<Camera> camera)
{
	cameras.push_back(camera);

	// Make this the current camera if we have none
	if (currentCamera == 0)
		currentCamera = camera;
}

void Scene::AddLight(Light light)
{
	lights.push_back(light);
}

void Scene::SetSky(std::shared_ptr<Sky> sky)
{
}

void Scene::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	// Ensure we have something to do
	if (currentCamera == 0)
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
