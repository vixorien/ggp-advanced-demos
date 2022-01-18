#include "GameEntity.h"

using namespace DirectX;

GameEntity::GameEntity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material)
{
	// Save the data
	this->mesh = mesh;
	this->material = material;
}

std::shared_ptr<Mesh> GameEntity::GetMesh() { return mesh; }
std::shared_ptr<Material> GameEntity::GetMaterial() { return material; }
Transform* GameEntity::GetTransform() { return &transform; }


void GameEntity::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera)
{
	// Tell the material to prepare for a draw
	material->PrepareMaterial(&transform, camera);

	// Draw the mesh
	mesh->SetBuffersAndDraw(context);
}
