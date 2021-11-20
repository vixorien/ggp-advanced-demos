#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include "Mesh.h"
#include "Transform.h"
#include "Camera.h"

class GameEntity
{
public:
	GameEntity(std::shared_ptr<Mesh> mesh);

	std::shared_ptr<Mesh> GetMesh();
	void SetMesh(std::shared_ptr<Mesh> mesh);

	Transform* GetTransform();

private:

	std::shared_ptr<Mesh> mesh;
	Transform transform;
};

