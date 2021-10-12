#include "Camera.h"
#include "Input.h"

using namespace DirectX;

// Creates a camera at the specified position
Camera::Camera(float x, float y, float z, float moveSpeed, float mouseLookSpeed, float aspectRatio)
{
	this->movementSpeed = moveSpeed;
	this->mouseLookSpeed = mouseLookSpeed;
	transform.SetPosition(x, y, z);

	UpdateViewMatrix();
	UpdateProjectionMatrix(aspectRatio);
}

// Nothing to really do
Camera::~Camera()
{ }


// Camera's update, which looks for key presses
void Camera::Update(float dt)
{
	// Current speed
	float speed = dt * movementSpeed;

	// Get the input manager instance
	Input& input = Input::GetInstance();

	// Speed up or down as necessary
	if (input.KeyDown(VK_SHIFT)) { speed *= 5; }
	if (input.KeyDown(VK_CONTROL)) { speed *= 0.1f; }

	// Movement
	if (input.KeyDown('W')) { transform.MoveRelative(0, 0, speed); }
	if (input.KeyDown('S')) { transform.MoveRelative(0, 0, -speed); }
	if (input.KeyDown('A')) { transform.MoveRelative(-speed, 0, 0); }
	if (input.KeyDown('D')) { transform.MoveRelative(speed, 0, 0); }
	if (input.KeyDown('X')) { transform.MoveAbsolute(0, -speed, 0); }
	if (input.KeyDown(' ')) { transform.MoveAbsolute(0, speed, 0); }

	// Handle mouse movement only when button is down
	if (input.MouseLeftDown())
	{
		// Calculate cursor change
		float xDiff = dt * mouseLookSpeed * input.GetMouseXDelta();
		float yDiff = dt * mouseLookSpeed * input.GetMouseYDelta();
		transform.Rotate(yDiff, xDiff, 0);
	}

	// Update the view every frame - could be optimized
	UpdateViewMatrix();

}

// Creates a new view matrix based on current position and orientation
void Camera::UpdateViewMatrix()
{
	// Rotate the standard "forward" matrix by our rotation
	// This gives us our "look direction"
	XMFLOAT3 rot = transform.GetPitchYawRoll();
	XMVECTOR dir = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&rot)));

	XMFLOAT3 pos = transform.GetPosition();
	XMMATRIX view = XMMatrixLookToLH(
		XMLoadFloat3(&pos),
		dir,
		XMVectorSet(0, 1, 0, 0));

	XMStoreFloat4x4(&viewMatrix, view);
}

// Updates the projection matrix
void Camera::UpdateProjectionMatrix(float aspectRatio)
{
	XMMATRIX P = XMMatrixPerspectiveFovLH(
		0.25f * XM_PI,		// Field of View Angle
		aspectRatio,		// Aspect ratio
		0.01f,				// Near clip plane distance
		100.0f);			// Far clip plane distance
	XMStoreFloat4x4(&projMatrix, P);
}

Transform* Camera::GetTransform()
{
	return &transform;
}
