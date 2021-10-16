#pragma once

#include <DirectXMath.h>
#include <vector>

class Transform
{
public:
	Transform();

	void MoveAbsolute(float x, float y, float z);
	void MoveRelative(float x, float y, float z);
	void Rotate(float p, float y, float r);
	void Scale(float x, float y, float z);

	void SetPosition(float x, float y, float z);
	void SetRotation(float p, float y, float r);
	void SetScale(float x, float y, float z);

	void SetTransformsFromMatrix(DirectX::XMFLOAT4X4 worldMatrix);

	DirectX::XMFLOAT3 GetPosition();
	DirectX::XMFLOAT3 GetPitchYawRoll();
	DirectX::XMFLOAT3 GetScale();
	DirectX::XMFLOAT4X4 GetWorldMatrix();
	DirectX::XMFLOAT4X4 GetWorldInverseTransposeMatrix();

	// Local direction vector getters
	DirectX::XMFLOAT3 GetUp();
	DirectX::XMFLOAT3 GetRight();
	DirectX::XMFLOAT3 GetForward();

	void AddChild(Transform* child, bool makeChildRelative = true);
	void RemoveChild(Transform* child, bool applyParentTransform = true);
	void SetParent(Transform* newParent, bool makeChildRelative = true);
	Transform* GetParent();
	Transform* GetChild(unsigned int index);
	int IndexOfChild(Transform* child);
	unsigned int GetChildCount();

private:
	// Hierarchy
	Transform* parent;
	std::vector<Transform*> children;

	// Raw transformation data
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 pitchYawRoll;
	DirectX::XMFLOAT3 scale;

	// Local orientation vectors
	bool vectorsDirty;
	DirectX::XMFLOAT3 up;
	DirectX::XMFLOAT3 right;
	DirectX::XMFLOAT3 forward;

	// World matrix and inverse transpose of the world matrix
	bool matricesDirty;
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 worldInverseTransposeMatrix;

	// Helper to update both matrices if necessary
	void UpdateMatrices();
	void UpdateVectors();
	void MarkChildTransformsDirty();

	// Helpers for conversion
	DirectX::XMFLOAT3 QuaternionToEuler(DirectX::XMFLOAT4 quaternion);
};

