#pragma once
#include <DirectXMath.h>
#include <Windows.h>

#include "Transform.h"

class Camera
{
public:
	Camera(float x, float y, float z, float moveSpeed, float mouseLookSpeed, float aspectRatio);
	~Camera();

	// Updating
	void Update(float dt);
	void UpdateViewMatrix();
	void UpdateProjectionMatrix(float aspectRatio);

	// Getters
	DirectX::XMFLOAT4X4 GetView() { return viewMatrix; }
	DirectX::XMFLOAT4X4 GetProjection() { return projMatrix; }

	Transform* GetTransform();

private:
	// Camera matrices
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projMatrix;

	Transform transform;

	float movementSpeed;
	float mouseLookSpeed;
};

