#include "FluidField.h"
#include "SimpleShader.h"
#include "Assets.h"
#include "Mesh.h"

#include <DirectXPackedVector.h>

using namespace DirectX;

// References:
// https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids
// http://web.stanford.edu/class/cs237d/smoke.pdf


FluidField::FluidField(
	Microsoft::WRL::ComPtr<ID3D11Device> device, 
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
	unsigned int gridSizeX,
	unsigned int gridSizeY,
	unsigned int gridSizeZ) 
	:
	device(device),
	context(context),
	gridSizeX(gridSizeX),
	gridSizeY(gridSizeY),
	gridSizeZ(gridSizeZ),
	simType(FLUID_SIMULATION_TYPE::SMOKE),
	timeCounter(0.0f),
	pause(false),
	injectSmoke(true),
	applyVorticity(true),
	pressureIterations(30),
	raymarchSamples(128),
	fixedTimeStep(0.016f),
	ambientTemperature(0.0f),
	injectTemperature(0.5f),
	injectDensity(0.05f),
	injectRadius(0.15f),
	injectPosition(0.5f, 0.2f, 0.5f),
	injectVelocityImpulseScale(5.0f),
	injectVelocityImpulse(0,0,0),
	temperatureBuoyancy(0.5f),
	densityWeight(0.1f),
	velocityDamper(1.0f),
	densityDamper(1.0f),
	temperatureDamper(1.0f),
	fluidColor(1.0f, 1.0f, 1.0f),
	vorticityEpsilon(0.3f),
	renderBuffer(FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_DENSITY),
	renderMode(FLUID_RENDER_MODE::FLUID_RENDER_MODE_BLEND)
{
	// Check for obstacle voxelization capabilities (DX11.3 feature
	// that allows for render target array index in the vertex shader)
	D3D11_FEATURE_DATA_D3D11_OPTIONS3 options;
	device->CheckFeatureSupport(
		D3D11_FEATURE_D3D11_OPTIONS3,
		&options,
		sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS3));
	obstaclesEnabled = (bool)options.VPAndRTArrayIndexFromAnyShaderFeedingRasterizer;

	// Set up buffers
	RecreateGPUResources();

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	device->CreateSamplerState(&sampDesc, samplerLinearClamp.GetAddressOf());

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = true;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS;
	device->CreateDepthStencilState(&depthDesc, depthState.GetAddressOf());

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blendDesc, blendState.GetAddressOf());

	D3D11_RASTERIZER_DESC rasterDesc = {};
	rasterDesc.CullMode = D3D11_CULL_FRONT;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.DepthClipEnable = true;
	device->CreateRasterizerState(&rasterDesc, rasterState.GetAddressOf());

}

FluidField::~FluidField()
{
}


void FluidField::RecreateGPUResources()
{
	velocityBuffers[0].Reset();
	velocityBuffers[1].Reset();
	divergenceBuffer.Reset();
	pressureBuffers[0].Reset();
	pressureBuffers[1].Reset();
	densityBuffers[0].Reset();
	densityBuffers[1].Reset();
	temperatureBuffers[0].Reset();
	temperatureBuffers[1].Reset();
	vorticityBuffer.Reset();
	obstacleBuffer.Reset();
	levelSetBuffers[0].Reset();
	levelSetBuffers[1].Reset();

	velocityBuffers[0] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32G32B32A32_FLOAT);
	velocityBuffers[1] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32G32B32A32_FLOAT);
	divergenceBuffer = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32_FLOAT);
	pressureBuffers[0] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32_FLOAT);
	pressureBuffers[1] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32_FLOAT);
	densityBuffers[0] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32G32B32A32_FLOAT);
	densityBuffers[1] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32G32B32A32_FLOAT);
	temperatureBuffers[0] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32_FLOAT);
	temperatureBuffers[1] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32_FLOAT);
	vorticityBuffer = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32G32B32A32_FLOAT);
	levelSetBuffers[0] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32_FLOAT);
	levelSetBuffers[1] = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R32_FLOAT);

	// Obstacle for testing
	unsigned int dataSize = gridSizeX * gridSizeY * gridSizeZ;
	unsigned char* obData = new unsigned char[dataSize];
	for (unsigned int z = 0; z < gridSizeZ; z++)
		for (unsigned int y = 0; y < gridSizeY; y++)
			for (unsigned int x = 0; x < gridSizeX; x++)
			{
				// Using min() to remove buffer overrun warning
				int index = min(x + (gridSizeX * y) + (gridSizeX * gridSizeY * z), dataSize - 1);
				obData[index] = (unsigned char)0;

				// Sphere
				{
					//// Get distance from "center"
					//float xDist = abs(32.0f - x);
					//float yDist = abs(32.0f - y);
					//float zDist = abs(32.0f - z);

					//float dist = sqrt(xDist * xDist + yDist * yDist + zDist * zDist);

					//if (dist <= 7.0f)
					//	obData[index] = 255;
				}

				// Flat cube
				{
				/*	if (x > 10 && x < gridSize - 10 && z > 10 && z < gridSize - 10 && y == 32)
					{
						obData[index] = (unsigned char)255;
					}*/
				}

				// NxNxN cube
				{
					/*int halfSize = 2;
					if ((x >= 32 - halfSize && x < 32 + halfSize) &&
						(y >= 32 - halfSize && y < 32 + halfSize) &&
						(z >= 32 - halfSize && z < 32 + halfSize))
						obData[index] = 255;*/
				}

				// Single cell(s)
				{
					/*if((x == 32 || x == 33) && y == 32 && z == 32)
						obData[index] = 255;*/
				}
			}

	obstacleBuffer = CreateVolumeResource(gridSizeX, gridSizeY, gridSizeZ, DXGI_FORMAT_R8_UNORM, obData);
	delete[] obData;

	// Should we make voxelization resources?
	if (obstaclesEnabled)
	{
		// Create array of depth stencil buffers for object voxelization if necessary
		D3D11_TEXTURE2D_DESC stencilDesc = {};
		{	// Size
			stencilDesc.Width = gridSizeX;
			stencilDesc.Height = gridSizeY;
			stencilDesc.ArraySize = gridSizeZ; // Forward-facing for now, but might change to "smallest dimension"
		}
		stencilDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
		stencilDesc.CPUAccessFlags = 0;
		stencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		stencilDesc.MipLevels = 1;
		stencilDesc.MiscFlags = 0;
		stencilDesc.SampleDesc.Count = 1;
		stencilDesc.SampleDesc.Quality = 0;
		stencilDesc.Usage = D3D11_USAGE_DEFAULT;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> stencilBuffer;
		device->CreateTexture2D(&stencilDesc, 0, stencilBuffer.GetAddressOf());

		// Create the view
		D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		viewDesc.Texture2DArray.ArraySize = stencilDesc.ArraySize;
		viewDesc.Texture2DArray.FirstArraySlice = 0;
		viewDesc.Texture2DArray.MipSlice = 0;
		viewDesc.Format = stencilDesc.Format;
		device->CreateDepthStencilView(stencilBuffer.Get(), &viewDesc, voxelizationDepthStencilView.GetAddressOf());
		
		// Create the array of projection matrices

	}


	// Unused, but for reference...

	// Note the usage of PackedVector::XMUBYTEN4, which corresponds to R8G8B8A8.  The constructor
	// also converts from the 0.0-1.0 float range to the 0-255 uint range
	// See here for other types: https://docs.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-internals#graphics-library-type-equivalence

	// TEMP: Some initial data for testing the volumes!
	/*float invDim = 1.0f / gridSize;
	PackedVector::XMUBYTEN4* colors = new PackedVector::XMUBYTEN4[gridSize * gridSize * gridSize];
	for (unsigned int x = 0; x < gridSize; x++)
		for (unsigned int y = 0; y < gridSize; y++)
			for (unsigned int z = 0; z < gridSize; z++)
			{
				int index = x + (gridSize * y) + (gridSize * gridSize * z);
				colors[index] = PackedVector::XMUBYTEN4(x * invDim, y * invDim, z * invDim, 1.0f);
			}*/

}


void FluidField::UpdateFluid(float deltaTime)
{
	// Don't run if paused
	if (pause)
		return;

	// Pile up the time
	timeCounter += deltaTime;
	if (timeCounter < fixedTimeStep)
		return;

	// Run a single time step
	OneTimeStep();

	// Apply one time step
	timeCounter -= fixedTimeStep;
}


void FluidField::OneTimeStep()
{
	//// Add smoke to the field
	//if (injectSmoke)
	//	InjectSmoke();

	// Apply the buoyancy force and advect velocity
	/*Buoyancy();*/
	Advection(velocityBuffers, velocityDamper);

	// Advect other quantities
	Advection(densityBuffers, densityDamper);
	Advection(temperatureBuffers, temperatureDamper);

	if (injectSmoke)
		InjectSmoke();

	Buoyancy();

	// Check for vorticity
	if (applyVorticity)
	{
		Vorticity();
		Confinement();
	}

	// Final fluid steps
	Divergence();
	Pressure();
	Projection();

	//// Advect other quantities
	//Advection(densityBuffers, densityDamper);
	//Advection(temperatureBuffers, temperatureDamper);
}


void FluidField::RenderFluid(Camera* camera)
{
	// Set up render states
	context->OMSetDepthStencilState(depthState.Get(), 0);
	context->OMSetBlendState(blendState.Get(), 0, 0xFFFFFFFF);
	context->RSSetState(rasterState.Get());

	// Get smallest dimension and use that for scaling
	float smallestDimension = (float)min(gridSizeX, min(gridSizeY, gridSizeZ));
	XMFLOAT3 scale = {
		2 * gridSizeX / smallestDimension,
		2 * gridSizeY / smallestDimension,
		2 * gridSizeZ / smallestDimension
	};

	// Cube location
	XMFLOAT3 translation(0, 0, 0);

	Assets& assets = Assets::GetInstance();
	SimplePixelShader* volumePS = assets.GetPixelShader("VolumePS.cso");
	SimpleVertexShader* volumeVS = assets.GetVertexShader("VolumeVS.cso");

	volumePS->SetShader();
	volumeVS->SetShader();

	// Vertex shader data
	XMMATRIX worldMat =
		XMMatrixScaling(scale.x, scale.y, scale.z) *
		XMMatrixTranslation(translation.x, translation.y, translation.z);

	XMFLOAT4X4 world, invWorld;
	XMStoreFloat4x4(&world, worldMat);
	XMStoreFloat4x4(&invWorld, XMMatrixInverse(0, worldMat));
	volumeVS->SetMatrix4x4("world", world);
	volumeVS->SetMatrix4x4("view", camera->GetView());
	volumeVS->SetMatrix4x4("projection", camera->GetProjection());
	volumeVS->CopyAllBufferData();

	// Resources
	volumePS->SetSamplerState("SamplerLinearClamp", samplerLinearClamp);
	
	// Assume debug mode unless we're doing the density buffer
	int modeOverride = -1;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	switch (renderBuffer)
	{
	default:
	case FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_DENSITY:
		srv = densityBuffers[0].SRV;
		modeOverride = (int)renderMode;
		break;

	case FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_VELOCITY: srv = velocityBuffers[0].SRV; break;
	case FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_DIVERGENCE: srv = divergenceBuffer.SRV; break;
	case FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_PRESSURE: srv = pressureBuffers[0].SRV; break;
	case FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_TEMPERATURE: srv = temperatureBuffers[0].SRV; break;
	case FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_VORTICITY: srv = vorticityBuffer.SRV; break;
	case FLUID_RENDER_BUFFER::FLUID_RENDER_BUFFER_OBSTACLES: srv = obstacleBuffer.SRV; break;
	}
	volumePS->SetShaderResourceView("volumeTexture", srv);

	// Pixel shader data
	volumePS->SetMatrix4x4("invWorld", invWorld);
	volumePS->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
	volumePS->SetFloat3("fluidColor", fluidColor);
	volumePS->SetInt("renderMode", modeOverride);
	volumePS->SetInt("raymarchSamples", raymarchSamples);
	volumePS->CopyAllBufferData();

	// Draw the geometry for the volume
	Mesh* cube = assets.GetMesh("Models\\cube.obj");
	cube->SetBuffersAndDraw(context);

	// Reset render states
	context->OMSetDepthStencilState(0, 0);
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->RSSetState(0);
}


void FluidField::VoxelizeObstacle(GameEntity* entity)
{
	// Able to use obstacles?
	if (!obstaclesEnabled)
		return;

	// Set up stencil states and proper stencil buffer
	
	
	// Turn on proper vertex shader and disable pixel shader


	// Set entity's matrix and custom view/projection matrices
	// - See this article for info on infinite far planes: https://ubm-twvideo01.s3.amazonaws.com/o1/vault/gdc07/slides/S3730i1.pdf

	// Draw the entity using instance rendering (one instance per "slice")


	// Copy results to obstacle buffer


	// Reset states

}

unsigned int FluidField::GetGridSizeX() { return gridSizeX; }
unsigned int FluidField::GetGridSizeY() { return gridSizeY; }
unsigned int FluidField::GetGridSizeZ() { return gridSizeZ; }
void FluidField::SetGridSize(unsigned int gridSizeX, unsigned int gridSizeY, unsigned int gridSizeZ)
{
	// Need more than 1 grid cell (probably even more)
	if (gridSizeX <= 1 || gridSizeY <= 1 || gridSizeZ <= 1)
		return;

	// Save and recreate resources
	this->gridSizeX = gridSizeX;
	this->gridSizeY = gridSizeY;
	this->gridSizeZ = gridSizeZ;
	RecreateGPUResources();
}

DirectX::XMFLOAT3 FluidField::GetInjectPosition() {	return injectPosition; }

void FluidField::SetInjectPosition(DirectX::XMFLOAT3 newPos, bool applyVelocityImpulse)
{
	// Should this position change cause velocity?
	if (applyVelocityImpulse)
	{
		// Note: Scaling velocity impulse by grid size because otherwise
		// it's way too small and the scale is required to be massive
		XMStoreFloat3(&injectVelocityImpulse, 
			XMLoadFloat3(&injectVelocityImpulse) + 
			(XMLoadFloat3(&newPos) - XMLoadFloat3(&injectPosition)) * XMVectorSet((float)gridSizeX, (float)gridSizeY, (float)gridSizeZ, 0) * injectVelocityImpulseScale);
	}

	// Update position
	injectPosition = newPos;
}



void FluidField::SwapBuffers(VolumeResource volumes[2])
{
	VolumeResource vr0 = volumes[0];
	volumes[0] = volumes[1];
	volumes[1] = vr0;
}

VolumeResource FluidField::CreateVolumeResource(unsigned int sizeX, unsigned int sizeY, unsigned int sizeZ, DXGI_FORMAT format, void* initialData)
{
	// Subresource data to fill with colors
	D3D11_SUBRESOURCE_DATA data = {};
	if (initialData)
	{
		data.pSysMem = initialData;
		data.SysMemPitch = DXGIFormatBytes(format) * sizeX;
		data.SysMemSlicePitch = DXGIFormatBytes(format) * sizeX * sizeY;
	}

	// Describe the texture itself
	D3D11_TEXTURE3D_DESC desc = {};
	desc.Width = sizeX;
	desc.Height = sizeY;
	desc.Depth = sizeZ;
	desc.Format = format;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.MipLevels = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;

	Microsoft::WRL::ComPtr<ID3D11Texture3D> texture;
	device->CreateTexture3D(&desc, initialData ? &data : 0, texture.GetAddressOf());

	// Struct to hold both resource views
	VolumeResource vr;
	vr.ChannelCount = DXGIFormatChannels(format);
	device->CreateShaderResourceView(texture.Get(), 0, vr.SRV.GetAddressOf());
	device->CreateUnorderedAccessView(texture.Get(), 0, vr.UAV.GetAddressOf());
	return vr;
}

void FluidField::Advection(VolumeResource volumes[2], float damper)
{
	// Grab the advection shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* advectCS = assets.GetComputeShader("AdvectionCS.cso");

	// Turn on and set external data
	advectCS->SetShader();
	advectCS->SetFloat("deltaTime", fixedTimeStep);
	advectCS->SetInt("gridSizeX", gridSizeX);
	advectCS->SetInt("gridSizeY", gridSizeY);
	advectCS->SetInt("gridSizeZ", gridSizeZ);
	advectCS->SetInt("channelCount", volumes[1].ChannelCount);
	advectCS->SetFloat("damper", damper);
	advectCS->CopyAllBufferData();

	// Set resources
	advectCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	advectCS->SetShaderResourceView("AdvectionIn", volumes[0].SRV);
	advectCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);
	advectCS->SetSamplerState("SamplerLinearClamp", samplerLinearClamp);
	switch (volumes[1].ChannelCount)
	{
	case 1: advectCS->SetUnorderedAccessView("AdvectionOut1", volumes[1].UAV); break;
	case 2: advectCS->SetUnorderedAccessView("AdvectionOut2", volumes[1].UAV); break;
	case 3: advectCS->SetUnorderedAccessView("AdvectionOut3", volumes[1].UAV); break;
	case 4: advectCS->SetUnorderedAccessView("AdvectionOut4", volumes[1].UAV); break;
	default: return;
	}

	// Run compute
	advectCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

	// Unset resources
	advectCS->SetShaderResourceView("VelocityIn", 0);
	advectCS->SetShaderResourceView("AdvectionIn", 0);
	advectCS->SetShaderResourceView("ObstaclesIn", 0);
	switch (volumes[1].ChannelCount)
	{
	case 1: advectCS->SetUnorderedAccessView("AdvectionOut1", 0); break;
	case 2: advectCS->SetUnorderedAccessView("AdvectionOut2", 0); break;
	case 3: advectCS->SetUnorderedAccessView("AdvectionOut3", 0); break;
	case 4: advectCS->SetUnorderedAccessView("AdvectionOut4", 0); break;
	}

	// Swap buffers
	SwapBuffers(volumes);
}

void FluidField::Divergence()
{
	// Grab the divergence shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* divCS = assets.GetComputeShader("DivergenceCS.cso");

	// Turn on
	divCS->SetShader(); 
	divCS->SetInt("gridSizeX", gridSizeX);
	divCS->SetInt("gridSizeY", gridSizeY);
	divCS->SetInt("gridSizeZ", gridSizeZ);
	divCS->CopyAllBufferData();

	// Set resources
	divCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	divCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);
	divCS->SetUnorderedAccessView("DivergenceOut", divergenceBuffer.UAV);

	// Run compute
	divCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

	// Unset resources
	divCS->SetShaderResourceView("VelocityIn", 0);
	divCS->SetShaderResourceView("ObstaclesIn", 0);
	divCS->SetUnorderedAccessView("DivergenceOut", 0);
}

void FluidField::Pressure()
{
	// Grab the clear and pressure shaders
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* clearCS = assets.GetComputeShader("Clear3DTextureCS.cso");
	SimpleComputeShader* pressCS = assets.GetComputeShader("PressureCS.cso");

	// Clear --------
	clearCS->SetShader();
	clearCS->SetFloat4("clearColor", { 0,0,0,0 });
	clearCS->SetInt("channelCount", 1);
	clearCS->CopyAllBufferData();

	clearCS->SetUnorderedAccessView("ClearOut1", pressureBuffers[0].UAV);
	clearCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);
	clearCS->SetUnorderedAccessView("ClearOut1", 0);


	// Pressure -----

	// Turn on
	pressCS->SetShader();
	pressCS->SetInt("gridSizeX", gridSizeX);
	pressCS->SetInt("gridSizeY", gridSizeY);
	pressCS->SetInt("gridSizeZ", gridSizeZ);
	pressCS->CopyAllBufferData();

	// Set resources
	pressCS->SetShaderResourceView("DivergenceIn", divergenceBuffer.SRV);
	pressCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);

	// Run the pressure solver for several iterations
	for (int i = 0; i < pressureIterations; i++)
	{
		// Set pressures (which swap each iteration)
		pressCS->SetShaderResourceView("PressureIn", pressureBuffers[0].SRV);
		pressCS->SetUnorderedAccessView("PressureOut", pressureBuffers[1].UAV);

		// Run compute
		pressCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

		// Unset output for next iteration
		pressCS->SetUnorderedAccessView("PressureOut", 0);

		// Swap the pressure buffers for the next iteration
		SwapBuffers(pressureBuffers);
	}

	// Unset resources
	pressCS->SetShaderResourceView("DivergenceIn", 0);
	pressCS->SetShaderResourceView("PressureIn", 0);
	pressCS->SetShaderResourceView("ObstaclesIn", 0);
	pressCS->SetUnorderedAccessView("PressureOut", 0);
}

void FluidField::Projection()
{
	// Grab the projection shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* projCS = assets.GetComputeShader("ProjectionCS.cso");

	// Turn on
	projCS->SetShader();
	projCS->SetInt("gridSizeX", gridSizeX);
	projCS->SetInt("gridSizeY", gridSizeY);
	projCS->SetInt("gridSizeZ", gridSizeZ);
	projCS->CopyAllBufferData();

	// Set resources
	projCS->SetShaderResourceView("PressureIn", pressureBuffers[0].SRV);
	projCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	projCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);
	projCS->SetUnorderedAccessView("VelocityOut", velocityBuffers[1].UAV);

	// Run compute
	projCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

	// Unset resources
	projCS->SetShaderResourceView("PressureIn", 0);
	projCS->SetShaderResourceView("VelocityIn", 0);
	projCS->SetShaderResourceView("ObstaclesIn", 0);
	projCS->SetUnorderedAccessView("VelocityOut", 0);

	// Swap buffers
	SwapBuffers(velocityBuffers);
}

void FluidField::InjectSmoke()
{
	// Grab the inject shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* injCS = assets.GetComputeShader("InjectSmokeCS.cso");

	// Turn on and set data
	injCS->SetShader();
	injCS->SetInt("gridSizeX", gridSizeX);
	injCS->SetInt("gridSizeY", gridSizeY);
	injCS->SetInt("gridSizeZ", gridSizeZ);
	injCS->SetFloat("deltaTime", fixedTimeStep);
	injCS->SetFloat("injectRadius", injectRadius);
	injCS->SetFloat3("injectPosition", injectPosition);
	injCS->SetFloat3("injectColor", fluidColor);
	injCS->SetFloat("injectDensity", injectDensity);
	injCS->SetFloat("injectTemperature", injectTemperature);
	injCS->SetFloat3("injectVelocity", injectVelocityImpulse);
	injCS->CopyAllBufferData();

	// Set resources
	injCS->SetShaderResourceView("DensityIn", densityBuffers[0].SRV);
	injCS->SetShaderResourceView("TemperatureIn", temperatureBuffers[0].SRV);
	injCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);
	injCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	injCS->SetUnorderedAccessView("DensityOut", densityBuffers[1].UAV);
	injCS->SetUnorderedAccessView("TemperatureOut", temperatureBuffers[1].UAV);
	injCS->SetUnorderedAccessView("VelocityOut", velocityBuffers[1].UAV);

	// Run compute
	injCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

	// Unset resources
	injCS->SetShaderResourceView("DensityIn", 0);
	injCS->SetShaderResourceView("TemperatureIn", 0);
	injCS->SetShaderResourceView("ObstaclesIn", 0);
	injCS->SetShaderResourceView("VelocityIn", 0);
	injCS->SetUnorderedAccessView("DensityOut", 0);
	injCS->SetUnorderedAccessView("TemperatureOut", 0);
	injCS->SetUnorderedAccessView("VelocityOut", 0);

	// Swap buffers
	SwapBuffers(densityBuffers);
	SwapBuffers(temperatureBuffers);
	SwapBuffers(velocityBuffers);

	// Reset injection velocity impulse now that its been applied
	injectVelocityImpulse = {};
}

void FluidField::Buoyancy()
{
	// Grab the buoyancy shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* buoyCS = assets.GetComputeShader("BuoyancyCS.cso");

	// Turn on and set data
	buoyCS->SetShader();
	buoyCS->SetFloat("deltaTime", fixedTimeStep);
	buoyCS->SetFloat("densityWeight", densityWeight);
	buoyCS->SetFloat("temperatureBuoyancy", temperatureBuoyancy);
	buoyCS->SetFloat("ambientTemperature", ambientTemperature);
	buoyCS->CopyAllBufferData();

	// Set resources
	buoyCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	buoyCS->SetShaderResourceView("DensityIn", densityBuffers[0].SRV);
	buoyCS->SetShaderResourceView("TemperatureIn", temperatureBuffers[0].SRV);
	buoyCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);
	buoyCS->SetUnorderedAccessView("VelocityOut", velocityBuffers[1].UAV);

	// Run compute
	buoyCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

	// Unset resources
	buoyCS->SetShaderResourceView("VelocityIn", 0);
	buoyCS->SetShaderResourceView("DensityIn", 0);
	buoyCS->SetShaderResourceView("TemperatureIn", 0);
	buoyCS->SetShaderResourceView("ObstaclesIn", 0);
	buoyCS->SetUnorderedAccessView("VelocityOut", 0);

	// Swap buffers
	SwapBuffers(velocityBuffers);
}

void FluidField::Vorticity()
{
	// Grab the projection shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* vortCS = assets.GetComputeShader("VorticityCS.cso");

	// Turn on
	vortCS->SetShader();
	vortCS->SetInt("gridSizeX", gridSizeX);
	vortCS->SetInt("gridSizeY", gridSizeY);
	vortCS->SetInt("gridSizeZ", gridSizeZ);
	vortCS->CopyAllBufferData();

	// Set resources
	vortCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	vortCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);
	vortCS->SetUnorderedAccessView("VorticityOut", vorticityBuffer.UAV);

	// Run compute
	vortCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

	// Unset resources
	vortCS->SetShaderResourceView("VelocityIn", 0);
	vortCS->SetShaderResourceView("ObstaclesIn", 0);
	vortCS->SetUnorderedAccessView("VorticityOut", 0);
}

void FluidField::Confinement()
{
	// Grab the projection shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* confCS = assets.GetComputeShader("ConfinementCS.cso");

	// Turn on
	confCS->SetShader();
	confCS->SetFloat("deltaTime", fixedTimeStep);
	confCS->SetInt("gridSizeX", gridSizeX);
	confCS->SetInt("gridSizeY", gridSizeY);
	confCS->SetInt("gridSizeZ", gridSizeZ);
	confCS->SetFloat("vorticityEpsilon", vorticityEpsilon);
	confCS->CopyAllBufferData();

	// Set resources
	confCS->SetShaderResourceView("VorticityIn", vorticityBuffer.SRV);
	confCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	confCS->SetShaderResourceView("ObstaclesIn", obstacleBuffer.SRV);
	confCS->SetUnorderedAccessView("VelocityOut", velocityBuffers[1].UAV);

	// Run compute
	confCS->DispatchByThreads(gridSizeX, gridSizeY, gridSizeZ);

	// Unset resources
	confCS->SetShaderResourceView("VorticityIn", 0);
	confCS->SetShaderResourceView("VelocityIn", 0);
	confCS->SetShaderResourceView("ObstaclesIn", 0);
	confCS->SetUnorderedAccessView("VelocityOut", 0);

	// Swap buffers
	SwapBuffers(velocityBuffers);
}


// From DirectXTex library
unsigned int FluidField::DXGIFormatBits(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 128;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 96;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_Y416:
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		return 64;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_Y410:
	case DXGI_FORMAT_YUY2:
		return 32;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		return 24;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_A8P8:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		return 16;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_NV11:
		return 12;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_AI44:
	case DXGI_FORMAT_IA44:
	case DXGI_FORMAT_P8:
		return 8;

	case DXGI_FORMAT_R1_UNORM:
		return 1;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 4;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return 8;

	default:
		return 0;
	}
}

unsigned int FluidField::DXGIFormatBytes(DXGI_FORMAT format)
{
	unsigned int bits = DXGIFormatBits(format);
	if (bits == 0)
		return 0;

	return max(1, bits / 8);
}

unsigned int FluidField::DXGIFormatChannels(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:	
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:	
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		return 4;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_B5G6R5_UNORM:
		return 3;


	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:	
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_A8P8:
		return 2;

	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_Y410:
	case DXGI_FORMAT_YUY2:
	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_Y416:
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_NV11:
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_AI44:
	case DXGI_FORMAT_IA44:
	case DXGI_FORMAT_P8:
	case DXGI_FORMAT_R1_UNORM:
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return 1;

	default:
		return 0;
	}
}


