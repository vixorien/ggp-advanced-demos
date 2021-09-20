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
	GameEntity(Mesh* mesh, Material* material);

	Mesh* GetMesh();
	Material* GetMaterial();
	Transform* GetTransform();

	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Camera* camera);

private:

	Mesh* mesh;
	Material* material;
	Transform transform;
};

