#include "Transform.h"

using namespace DirectX;


Transform::Transform()
{
	// Start with an identity matrix and basic transform data
	XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixIdentity());

	position = XMFLOAT3(0, 0, 0);
	pitchYawRoll = XMFLOAT3(0, 0, 0);
	scale = XMFLOAT3(1, 1, 1);

	up = XMFLOAT3(0, 1, 0);
	right = XMFLOAT3(1, 0, 0);
	forward = XMFLOAT3(0, 0, 1);

	// No need to recalc yet
	matricesDirty = false;
	vectorsDirty = false;


	// No parent or children yet
	parent = 0;
}

void Transform::MoveAbsolute(float x, float y, float z)
{
	position.x += x;
	position.y += y;
	position.z += z;
	matricesDirty = true;
}

void Transform::MoveRelative(float x, float y, float z)
{
	// Create a direction vector from the params
	// and a rotation quaternion
	XMVECTOR movement = XMVectorSet(x, y, z, 0);
	XMVECTOR rotQuat = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));

	// Rotate the movement by the quaternion
	XMVECTOR dir = XMVector3Rotate(movement, rotQuat);

	// Add and store, and invalidate the matrices
	XMStoreFloat3(&position, XMLoadFloat3(&position) + dir);
	matricesDirty = true;
}

void Transform::Rotate(float p, float y, float r)
{
	pitchYawRoll.x += p;
	pitchYawRoll.y += y;
	pitchYawRoll.z += r;
	matricesDirty = true;
	vectorsDirty = true;
}

void Transform::Scale(float x, float y, float z)
{
	scale.x *= x;
	scale.y *= y;
	scale.z *= z;
	matricesDirty = true;
}

void Transform::SetPosition(float x, float y, float z)
{
	position.x = x;
	position.y = y;
	position.z = z;
	matricesDirty = true;
}

void Transform::SetRotation(float p, float y, float r)
{
	pitchYawRoll.x = p;
	pitchYawRoll.y = y;
	pitchYawRoll.z = r;
	matricesDirty = true;
	vectorsDirty = true;
}

void Transform::SetScale(float x, float y, float z)
{
	scale.x = x;
	scale.y = y;
	scale.z = z;
	matricesDirty = true;
}

void Transform::SetTransformsFromMatrix(DirectX::XMFLOAT4X4 worldMatrix)
{
	// Decompose the matrix
	XMVECTOR localPos;
	XMVECTOR localRotQuat;
	XMVECTOR localScale;
	XMMatrixDecompose(&localScale, &localRotQuat, &localPos, XMLoadFloat4x4(&worldMatrix));

	// Get the euler angles from the quaternion and store as our 
	XMFLOAT4 quat;
	XMStoreFloat4(&quat, localRotQuat);
	pitchYawRoll = QuaternionToEuler(quat);

	// Overwrite the child's other transform data
	XMStoreFloat3(&position, localPos);
	XMStoreFloat3(&scale, localScale);

	// Things have changed
	matricesDirty = true;
	vectorsDirty = true;
}

DirectX::XMFLOAT3 Transform::GetPosition() { return position; }

DirectX::XMFLOAT3 Transform::GetPitchYawRoll() { return pitchYawRoll; }

DirectX::XMFLOAT3 Transform::GetScale() { return scale; }

DirectX::XMFLOAT3 Transform::GetUp()
{
	UpdateVectors();
	return up;
}

DirectX::XMFLOAT3 Transform::GetRight()
{
	UpdateVectors();
	return right;
}

DirectX::XMFLOAT3 Transform::GetForward()
{
	UpdateVectors();
	return forward;
}

DirectX::XMFLOAT4X4 Transform::GetWorldMatrix()
{
	UpdateMatrices();
	return worldMatrix;
}

DirectX::XMFLOAT4X4 Transform::GetWorldInverseTransposeMatrix()
{
	UpdateMatrices();
	return worldMatrix;
}

void Transform::AddChild(Transform* child, bool makeChildRelative)
{
	// Verify valid pointer
	if (!child) return;

	// Already a child?
	if (IndexOfChild(child) >= 0)
		return;

	// Do we need to adjust the child's transform
	// so that it stays in place?
	if (makeChildRelative)
	{
		// Get matrices
		XMFLOAT4X4 parentWorld = GetWorldMatrix();
		XMMATRIX pWorld = XMLoadFloat4x4(&parentWorld);

		XMFLOAT4X4 childWorld = child->GetWorldMatrix();
		XMMATRIX cWorld = XMLoadFloat4x4(&childWorld);

		// Invert the parent
		XMMATRIX pWorldInv = XMMatrixInverse(0, pWorld);

		// Multiply the child by the inverse parent
		XMMATRIX relCWorld = cWorld * pWorldInv;

		// Set the child's transform from this new matrix
		XMFLOAT4X4 relativeChildWorld;
		XMStoreFloat4x4(&relativeChildWorld, relCWorld);
		child->SetTransformsFromMatrix(relativeChildWorld);
	}

	// Reciprocal set!
	children.push_back(child);
	child->parent = this;

	// This child transform is now out of date
	child->matricesDirty = true;
	child->MarkChildTransformsDirty();
}

void Transform::RemoveChild(Transform* child, bool applyParentTransform)
{
	// Verify valid pointer
	if (!child) return;

	// Find the child
	auto it = std::find(children.begin(), children.end(), child);
	if (it == children.end())
		return;

	// Before actually un-parenting, are we applying the parent's transform?
	if (applyParentTransform)
	{
		// Grab the child's transform and matrix
		Transform* child = *it;
		XMFLOAT4X4 childWorld = child->GetWorldMatrix();

		// Set the child's transform data using its final matrix
		child->SetTransformsFromMatrix(childWorld);
	}

	// Reciprocal removal
	children.erase(it);
	child->parent = 0;

	// This child transform is now out of date
	child->matricesDirty = true;
	child->MarkChildTransformsDirty();
}

void Transform::SetParent(Transform* newParent, bool makeChildRelative)
{
	// Unparent if necessary
	if (this->parent)
	{
		// Remove this object from the parent's list
		// (which will also update our own parent reference!)
		this->parent->RemoveChild(this);
	}

	// Is the new parent something other than null?
	if (newParent)
	{
		// Add this object as a child
		newParent->AddChild(this, makeChildRelative);
	}
}

Transform* Transform::GetParent()
{
	return parent;
}

Transform* Transform::GetChild(unsigned int index)
{
	if (index >= children.size()) return 0;

	return children[index];
}

int Transform::IndexOfChild(Transform* child)
{
	// Verify pointer
	if (!child) return -1;

	// Search
	for (unsigned int i = 0; i < children.size(); i++)
		if (children[i] == child)
			return (int)i;

	// Not found
	return -1;
}

unsigned int Transform::GetChildCount()
{
	return (unsigned int)children.size();
}


void Transform::UpdateMatrices()
{
	// Are the matrices out of date (dirty)?
	if (matricesDirty)
	{
		// Create the three transformation pieces
		XMMATRIX trans = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
		XMMATRIX rot = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
		XMMATRIX sc = XMMatrixScalingFromVector(XMLoadFloat3(&scale));

		// Calculate the world matrix for this transform
		XMMATRIX wm = sc * rot * trans;

		// Is there a parent?
		if (parent)
		{
			XMFLOAT4X4 parentWorld = parent->GetWorldMatrix();
			wm *= XMLoadFloat4x4(&parentWorld);
		}

		// Save, then invert and transpose, too
		XMStoreFloat4x4(&worldMatrix, wm);
		XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixInverse(0, XMMatrixTranspose(wm)));

		// All set
		matricesDirty = false;

		// Notify all children that they're
		// now out of date
		MarkChildTransformsDirty();
	}
}

void Transform::UpdateVectors()
{
	// Do we need to update?
	if (!vectorsDirty)
		return;

	// Update all three vectors
	XMVECTOR rotationQuat = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
	XMStoreFloat3(&up, XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rotationQuat));
	XMStoreFloat3(&right, XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rotationQuat));
	XMStoreFloat3(&forward, XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rotationQuat));

	// Vectors are up to date
	vectorsDirty = false;
}

void Transform::MarkChildTransformsDirty()
{
	// Recursively set children as dirty, too
	for (auto c : children)
	{
		c->matricesDirty = true;
		c->MarkChildTransformsDirty();
	}
}

DirectX::XMFLOAT3 Transform::QuaternionToEuler(DirectX::XMFLOAT4 quaternion)
{
	// Convert quaternion to euler angles
	// Note: This is usually rough at best
	// 
	// Step 1: Quaternion to rotation matrix
	XMMATRIX rMat = XMMatrixRotationQuaternion(XMLoadFloat4(&quaternion));

	// Step 2: Extract each piece
	// From: https://stackoverflow.com/questions/60350349/directx-get-pitch-yaw-roll-from-xmmatrix
	XMFLOAT4X4 rotationMatrix;
	//XMStoreFloat4x4(&rotationMatrix, XMMatrixTranspose(rMat)); // Linked version used a transpose which seems unnecessary?
	//float pitch = (float)asin(-rotationMatrix._23);
	//float yaw = (float)atan2(rotationMatrix._13, rotationMatrix._33);
	//float roll = (float)atan2(rotationMatrix._21, rotationMatrix._22);

	// Version without transpose
	XMStoreFloat4x4(&rotationMatrix, rMat);
	float pitch = (float)asin(-rotationMatrix._32);
	float yaw = (float)atan2(rotationMatrix._31, rotationMatrix._33);
	float roll = (float)atan2(rotationMatrix._12, rotationMatrix._22);

	// Recreate quaternion to test
	//XMVECTOR testQuat = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);

	// Return the euler values as a vector
	return XMFLOAT3(pitch, yaw, roll);
}
