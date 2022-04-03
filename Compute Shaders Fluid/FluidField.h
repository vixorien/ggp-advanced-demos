#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Camera.h"

enum FLUID_TYPE
{
	FLUID_TYPE_SMOKE,
	FLUID_TYPE_WATER
};

struct VolumeResource
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> UAV;
};

class FluidField
{
public:
	FluidField(
		Microsoft::WRL::ComPtr<ID3D11Device> device, 
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
		unsigned int gridSize);
	~FluidField();

	void UpdateFluid(float deltaTime);
	void RenderFluid(Camera* camera);

private:

	// Field data
	unsigned int gridSize;
	unsigned int pressureIterations;

	// Volume textures
	VolumeResource velocityBuffers[2];
	VolumeResource divergenceBuffer;
	VolumeResource pressureBuffers[2];

	// DX Resources
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerLinearClamp;

	void SwapBuffers(VolumeResource volumes[2]);
	VolumeResource CreateVolumeResource(int sideDimension, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, void* initialData = 0);

	// Fluid functions
	void Advection(VolumeResource volumes[2], float deltaTime, float advectionDamper = 1.0f);
	void Divergence();
	void Pressure();
	void Projection();

	unsigned int DXGIFormatBits(DXGI_FORMAT format);
	unsigned int DXGIFormatBytes(DXGI_FORMAT format);
};

