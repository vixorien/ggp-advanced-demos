#include "GameEntity.h"
#include "BufferStructs.h"

using namespace DirectX;

GameEntity::GameEntity(std::shared_ptr<Mesh> mesh) :
	mesh(mesh)
{
}

std::shared_ptr<Mesh> GameEntity::GetMesh() { return mesh; }
void GameEntity::SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }

Transform* GameEntity::GetTransform() { return &transform; }
