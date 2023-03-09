#include "GameEntity.h"
#include "Assets.h"
#include "Helpers.h"

using namespace DirectX;

GameEntity::GameEntity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material) :
	mesh(mesh),
	material(material)
{
}

std::shared_ptr<Mesh> GameEntity::GetMesh() { return mesh; }
std::shared_ptr<Material> GameEntity::GetMaterial() { return material; }
Transform* GameEntity::GetTransform() { return &transform; }

void GameEntity::SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }
void GameEntity::SetMaterial(std::shared_ptr<Material> material) { this->material = material; }


void GameEntity::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera)
{
	// Set up the material (shaders)
	material->PrepareMaterial(&transform, camera);

	// Draw the mesh
	mesh->SetBuffersAndDraw(context);
}

std::shared_ptr<GameEntity> GameEntity::Parse(nlohmann::json jsonEntity)
{
	Assets& assets = Assets::GetInstance();

	std::shared_ptr<GameEntity> entity = std::make_shared<GameEntity>(
		assets.GetMesh(NarrowToWide(jsonEntity["mesh"].get<std::string>())),
		assets.GetMaterial(NarrowToWide(jsonEntity["material"].get<std::string>())));

	// Early out if transform is missing
	if (!jsonEntity.contains("transform")) return entity;
	nlohmann::json tr = jsonEntity["transform"];

	// Handle transform
	XMFLOAT3 pos = { 0, 0, 0 };
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
