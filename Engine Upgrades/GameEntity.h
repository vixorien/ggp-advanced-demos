#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include "Mesh.h"
#include "Material.h"
#include "Transform.h"
#include "Camera.h"
#include "SimpleShader.h"

class GameEntity
{
public:
	GameEntity(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material);

	std::shared_ptr<Mesh> GetMesh();
	std::shared_ptr<Material> GetMaterial();
	Transform* GetTransform();

	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera);

private:

	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<Material> material;
	Transform transform;
};

