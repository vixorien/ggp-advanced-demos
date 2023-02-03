#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Camera.h"
#include "GameEntity.h"

enum class FLUID_RENDER_BUFFER
{
	FLUID_RENDER_BUFFER_DENSITY,
	FLUID_RENDER_BUFFER_VELOCITY,
	FLUID_RENDER_BUFFER_DIVERGENCE,
	FLUID_RENDER_BUFFER_PRESSURE,
	FLUID_RENDER_BUFFER_TEMPERATURE,
	FLUID_RENDER_BUFFER_VORTICITY,
	FLUID_RENDER_BUFFER_OBSTACLES
};

enum class FLUID_RENDER_MODE
{
	FLUID_RENDER_MODE_BLEND,
	FLUID_RENDER_MODE_ADD
};

enum class FLUID_SIMULATION_TYPE
{
	SMOKE,
	WATER
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
		unsigned int gridSizeX,
		unsigned int gridSizeY,
		unsigned int gridSizeZ);
	~FluidField();

	void RecreateGPUResources();
	void UpdateFluid(float deltaTime);
	void OneTimeStep();
	void RenderFluid(Camera* camera);

	void VoxelizeObstacle(GameEntity* entity);

	// Publically accessible data
	bool pause;
	bool injectSmoke;
	bool applyVorticity;
	int pressureIterations;
	int raymarchSamples;
	float fixedTimeStep;
	float ambientTemperature;
	float injectTemperature;
	float injectDensity;
	float injectRadius;
	float injectVelocityImpulseScale;
	float temperatureBuoyancy;
	float densityWeight;
	float velocityDamper;
	float densityDamper;
	float temperatureDamper; 
	float vorticityEpsilon;
	DirectX::XMFLOAT3 fluidColor;
	FLUID_RENDER_BUFFER renderBuffer;
	FLUID_RENDER_MODE renderMode;

	unsigned int GetGridSizeX();
	unsigned int GetGridSizeY();
	unsigned int GetGridSizeZ();
    void SetGridSize(unsigned int gridSizeX, unsigned int gridSizeY, unsigned int gridSizeZ);
	DirectX::XMFLOAT3 GetInjectPosition();
	void SetInjectPosition(DirectX::XMFLOAT3 newPos, bool applyVelocityImpulse);

private:

	// Private field data
	FLUID_SIMULATION_TYPE simType;
	unsigned int gridSizeX;
	unsigned int gridSizeY;
	unsigned int gridSizeZ;
	float timeCounter;
	bool obstaclesEnabled;
	DirectX::XMFLOAT3 injectPosition;
	DirectX::XMFLOAT3 injectVelocityImpulse;

	// Volume textures for all fluids
	VolumeResource velocityBuffers[2];
	VolumeResource divergenceBuffer;
	VolumeResource pressureBuffers[2];

	// Smoke volume textures
	VolumeResource densityBuffers[2];
	VolumeResource temperatureBuffers[2];
	VolumeResource vorticityBuffer;

	// Obstacle textures
	VolumeResource obstacleBuffer;

	// Liquid textures
	VolumeResource levelSetBuffers[2];

	// DX Resources
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerLinearClamp;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterState;

	// Obstacle voxelization
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> voxelizationDepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> voxelizationDepthStencilView; // Texture array!
	DirectX::XMFLOAT4X4 voxelizationViewMatrix;
	DirectX::XMFLOAT4X4* voxelizationProjectionMatrices;

	// Helper methods
	void SwapBuffers(VolumeResource volumes[2]);
	VolumeResource CreateVolumeResource(
		unsigned int sizeX, 
		unsigned int sizeY, 
		unsigned int sizeZ, 
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, 
		void* initialData = 0);

	// Fluid functions
	void Advection(VolumeResource volumes[2], float damper = 1.0f);
	void Divergence();
	void Pressure();
	void Projection();
	void InjectSmoke();
	void Buoyancy();
	void Vorticity();
	void Confinement();

	// Format helpers
	unsigned int DXGIFormatBits(DXGI_FORMAT format);
	unsigned int DXGIFormatBytes(DXGI_FORMAT format);
	unsigned int DXGIFormatChannels(DXGI_FORMAT format);
};

