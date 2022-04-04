#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Camera.h"

enum FLUID_TYPE
{
	FLUID_TYPE_SMOKE,
	FLUID_TYPE_FIRE
};

enum class FLUID_RENDER_TYPE
{
	FLUID_RENDER_VELOCITY,
	FLUID_RENDER_DIVERGENCE,
	FLUID_RENDER_PRESSURE,
	FLUID_RENDER_DENSITY,
	FLUID_RENDER_TEMPERATURE
};

struct VolumeResource
{
	unsigned int ChannelCount{ 0 };
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> UAV;

	void Reset()
	{
		SRV.Reset();
		UAV.Reset();
	}
};

class FluidField
{
public:
	FluidField(
		Microsoft::WRL::ComPtr<ID3D11Device> device, 
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
		unsigned int gridSize);
	~FluidField();

	void RecreateGPUResources();
	void UpdateFluid(float deltaTime);
	void RenderFluid(Camera* camera);

	// Publically accessible data
	bool injectSmoke;
	int pressureIterations;
	float timeStepMultiplier;
	FLUID_RENDER_TYPE renderType;

private:

	// Field data
	unsigned int gridSize;

	// Volume textures for all fluids
	VolumeResource velocityBuffers[2];
	VolumeResource divergenceBuffer;
	VolumeResource pressureBuffers[2];

	// Smoke volume textures
	VolumeResource densityBuffers[2];
	VolumeResource temperatureBuffers[2];

	// DX Resources
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerLinearClamp;

	// Helper methods
	void SwapBuffers(VolumeResource volumes[2]);
	VolumeResource CreateVolumeResource(int sideDimension, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, void* initialData = 0);

	// Fluid functions
	void Advection(VolumeResource volumes[2], float deltaTime, float damper = 1.0f);
	void Divergence();
	void Pressure();
	void Projection();
	void InjectSmoke(float deltaTime);
	void Buoyancy(float deltaTime);

	// Format helpers
	unsigned int DXGIFormatBits(DXGI_FORMAT format);
	unsigned int DXGIFormatBytes(DXGI_FORMAT format);
	unsigned int DXGIFormatChannels(DXGI_FORMAT format);
};

