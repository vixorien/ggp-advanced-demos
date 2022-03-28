#pragma once
#include <DirectXMath.h>
#include <Windows.h>

#include "Transform.h"

class Camera
{
public:
	Camera(float x, float y, float z, float moveSpeed, float mouseLookSpeed, float aspectRatio, float nearClip = 0.01f, float farClip = 100.0f);
	~Camera();

	// Updating
	void Update(float dt);
	void UpdateViewMatrix();
	void UpdateProjectionMatrix(float aspectRatio);

	// Getters
	DirectX::XMFLOAT4X4 GetView() { return viewMatrix; }
	DirectX::XMFLOAT4X4 GetProjection() { return projMatrix; }

	Transform* GetTransform();

	float GetNearClip();
	float GetFarClip();
	void SetNearClip(float nearClip);
	void SetFarClip(float farClip);

private:
	// Camera matrices
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projMatrix;

	Transform transform;

	float movementSpeed;
	float mouseLookSpeed;

	float nearClip;
	float farClip;
};

