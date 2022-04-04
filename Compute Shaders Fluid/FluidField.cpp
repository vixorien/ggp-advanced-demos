#include "FluidField.h"
#include "SimpleShader.h"
#include "Assets.h"
#include "Mesh.h"

#include <DirectXPackedVector.h>

using namespace DirectX;

// References:
// https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids
// http://web.stanford.edu/class/cs237d/smoke.pdf
// TODO: Update based on paper above (GPU Gems has some inconsistencies)


FluidField::FluidField(Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, unsigned int gridSize) :
	device(device),
	context(context),
	gridSize(gridSize),
	injectSmoke(false),
	applyVorticity(false),
	pressureIterations(32),
	fixedTimeStep(0.1f),
	ambientTemperature(50.0f),
	injectTemperature(100.0f),
	injectDensity(0.1f),
	injectRadius(0.1f),
	injectPosition(0.5f, 0.2f, 0.5f),
	temperatureBuoyancy(0.1f),
	densityWeight(0.1f),
	velocityDamper(0.999f),
	densityDamper(0.999f),
	temperatureDamper(0.999f),
	fluidColor(1.0f, 1.0f, 1.0f),
	renderType(FLUID_RENDER_TYPE::FLUID_RENDER_DENSITY)
{

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

	velocityBuffers[0] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32G32B32A32_FLOAT);
	velocityBuffers[1] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32G32B32A32_FLOAT);
	divergenceBuffer = CreateVolumeResource(gridSize, DXGI_FORMAT_R32_FLOAT);
	pressureBuffers[0] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32_FLOAT);
	pressureBuffers[1] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32_FLOAT);
	densityBuffers[0] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32_FLOAT);
	densityBuffers[1] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32_FLOAT);
	temperatureBuffers[0] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32_FLOAT);
	temperatureBuffers[1] = CreateVolumeResource(gridSize, DXGI_FORMAT_R32_FLOAT);
	vorticityBuffer = CreateVolumeResource(gridSize, DXGI_FORMAT_R32G32B32A32_FLOAT);

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


void FluidField::UpdateFluid()
{
	// Add smoke to the field
	if(injectSmoke)
		InjectSmoke();

	// Advect the velocity and other quantities
	Buoyancy();
	Advection(velocityBuffers, velocityDamper);

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

	Advection(densityBuffers, densityDamper);
	Advection(temperatureBuffers, temperatureDamper);
}


void FluidField::RenderFluid(Camera* camera)
{
	// Set up render states
	context->OMSetDepthStencilState(depthState.Get(), 0);
	context->OMSetBlendState(blendState.Get(), 0, 0xFFFFFFFF);
	// TODO: Change cull mode?

	// Cube size
	XMFLOAT3 translation(0, 0, 0);
	XMFLOAT3 scale(2,2,2);

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
	
	bool debugDraw = true;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	switch (renderType)
	{
	default:
	case FLUID_RENDER_TYPE::FLUID_RENDER_VELOCITY: srv = velocityBuffers[0].SRV; break;
	case FLUID_RENDER_TYPE::FLUID_RENDER_DIVERGENCE: srv = divergenceBuffer.SRV; break;
	case FLUID_RENDER_TYPE::FLUID_RENDER_PRESSURE: srv = pressureBuffers[0].SRV; break;
	case FLUID_RENDER_TYPE::FLUID_RENDER_DENSITY: srv = densityBuffers[0].SRV; debugDraw = false; break;
	case FLUID_RENDER_TYPE::FLUID_RENDER_TEMPERATURE: srv = temperatureBuffers[0].SRV; break;
	case FLUID_RENDER_TYPE::FLUID_RENDER_VORTICITY: srv = vorticityBuffer.SRV; break;
	}
	volumePS->SetShaderResourceView("volumeTexture", srv);

	// Pixel shader data
	volumePS->SetMatrix4x4("invWorld", invWorld);
	volumePS->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
	volumePS->SetFloat3("fluidColor", fluidColor);
	volumePS->SetInt("debugRaymarchTexture", (int)debugDraw);
	volumePS->CopyAllBufferData();

	// Draw the geometry for the volume
	Mesh* cube = assets.GetMesh("Models\\cube.obj");
	cube->SetBuffersAndDraw(context);

	// Reset render states
	context->OMSetDepthStencilState(0, 0);
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
}



void FluidField::SwapBuffers(VolumeResource volumes[2])
{
	VolumeResource vr0 = volumes[0];
	volumes[0] = volumes[1];
	volumes[1] = vr0;
}

VolumeResource FluidField::CreateVolumeResource(int sideDimension, DXGI_FORMAT format, void* initialData)
{
	// Subresource data to fill with colors
	D3D11_SUBRESOURCE_DATA data = {};
	if (initialData)
	{
		data.pSysMem = initialData;
		data.SysMemPitch = DXGIFormatBytes(format) * sideDimension;
		data.SysMemSlicePitch = DXGIFormatBytes(format) * sideDimension * sideDimension;
	}

	// Describe the texture itself
	D3D11_TEXTURE3D_DESC desc = {};
	desc.Width = sideDimension;
	desc.Height = sideDimension;
	desc.Depth = sideDimension;
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
	advectCS->SetInt("gridSizeX", gridSize);
	advectCS->SetInt("gridSizeY", gridSize);
	advectCS->SetInt("gridSizeZ", gridSize);
	advectCS->SetInt("channelCount", volumes[1].ChannelCount);
	advectCS->SetFloat("damper", damper);
	advectCS->CopyAllBufferData();

	// Set resources
	advectCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	advectCS->SetShaderResourceView("AdvectionIn", volumes[0].SRV);
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
	advectCS->DispatchByThreads(gridSize, gridSize, gridSize);

	// Unset resources
	advectCS->SetShaderResourceView("VelocityIn", 0);
	advectCS->SetShaderResourceView("AdvectionIn", 0);
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
	divCS->SetInt("gridSizeX", gridSize);
	divCS->SetInt("gridSizeY", gridSize);
	divCS->SetInt("gridSizeZ", gridSize);
	divCS->CopyAllBufferData();

	// Set resources
	divCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	divCS->SetUnorderedAccessView("DivergenceOut", divergenceBuffer.UAV);

	// Run compute
	divCS->DispatchByThreads(gridSize, gridSize, gridSize);

	// Unset resources
	divCS->SetShaderResourceView("VelocityIn", 0);
	divCS->SetUnorderedAccessView("DivergenceOut", 0);
}

void FluidField::Pressure()
{
	// Grab the pressure shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* pressCS = assets.GetComputeShader("PressureCS.cso");

	// Turn on
	pressCS->SetShader();
	pressCS->SetInt("gridSizeX", gridSize);
	pressCS->SetInt("gridSizeY", gridSize);
	pressCS->SetInt("gridSizeZ", gridSize);
	pressCS->CopyAllBufferData();

	// Set resources
	pressCS->SetShaderResourceView("DivergenceIn", divergenceBuffer.SRV);

	// Run the pressure solver for several iterations
	for (int i = 0; i < pressureIterations; i++)
	{
		// Set pressures (which swap each iteration)
		pressCS->SetShaderResourceView("PressureIn", pressureBuffers[0].SRV);
		pressCS->SetUnorderedAccessView("PressureOut", pressureBuffers[1].UAV);

		// Run compute
		pressCS->DispatchByThreads(gridSize, gridSize, gridSize);

		// Unset output for next iteration
		pressCS->SetUnorderedAccessView("PressureOut", 0);

		// Swap the pressure buffers for the next iteration
		SwapBuffers(pressureBuffers);
	}

	// Unset resources
	pressCS->SetShaderResourceView("DivergenceIn", 0);
	pressCS->SetShaderResourceView("PressureIn", 0);
	pressCS->SetUnorderedAccessView("PressureOut", 0);
}

void FluidField::Projection()
{
	// Grab the projection shader
	Assets& assets = Assets::GetInstance();
	SimpleComputeShader* projCS = assets.GetComputeShader("ProjectionCS.cso");

	// Turn on
	projCS->SetShader();
	projCS->SetInt("gridSizeX", gridSize);
	projCS->SetInt("gridSizeY", gridSize);
	projCS->SetInt("gridSizeZ", gridSize);
	projCS->CopyAllBufferData();

	// Set resources
	projCS->SetShaderResourceView("PressureIn", pressureBuffers[0].SRV);
	projCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	projCS->SetUnorderedAccessView("VelocityOut", velocityBuffers[1].UAV);

	// Run compute
	projCS->DispatchByThreads(gridSize, gridSize, gridSize);

	// Unset resources
	projCS->SetShaderResourceView("PressureIn", 0);
	projCS->SetShaderResourceView("VelocityIn", 0);
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
	injCS->SetInt("gridSizeX", gridSize);
	injCS->SetInt("gridSizeY", gridSize);
	injCS->SetInt("gridSizeZ", gridSize);
	injCS->SetFloat("deltaTime", fixedTimeStep);
	injCS->SetFloat("injectRadius", injectRadius);
	injCS->SetFloat3("injectPosition", injectPosition);
	injCS->SetFloat("injectDensity", injectDensity);
	injCS->SetFloat("injectTemperature", injectTemperature);
	injCS->CopyAllBufferData();

	// Set resources
	injCS->SetShaderResourceView("DensityIn", densityBuffers[0].SRV);
	injCS->SetShaderResourceView("TemperatureIn", temperatureBuffers[0].SRV);
	injCS->SetUnorderedAccessView("DensityOut", densityBuffers[1].UAV);
	injCS->SetUnorderedAccessView("TemperatureOut", temperatureBuffers[1].UAV);

	// Run compute
	injCS->DispatchByThreads(gridSize, gridSize, gridSize);

	// Unset resources
	injCS->SetShaderResourceView("DensityIn", 0);
	injCS->SetShaderResourceView("TemperatureIn", 0);
	injCS->SetUnorderedAccessView("DensityOut", 0);
	injCS->SetUnorderedAccessView("TemperatureOut", 0);

	// Swap buffers
	SwapBuffers(densityBuffers);
	SwapBuffers(temperatureBuffers);
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
	buoyCS->SetUnorderedAccessView("VelocityOut", velocityBuffers[1].UAV);

	// Run compute
	buoyCS->DispatchByThreads(gridSize, gridSize, gridSize);

	// Unset resources
	buoyCS->SetShaderResourceView("VelocityIn", 0);
	buoyCS->SetShaderResourceView("TemperatureIn", 0);
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
	vortCS->SetInt("gridSizeX", gridSize);
	vortCS->SetInt("gridSizeY", gridSize);
	vortCS->SetInt("gridSizeZ", gridSize);
	vortCS->CopyAllBufferData();

	// Set resources
	vortCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	vortCS->SetUnorderedAccessView("VorticityOut", vorticityBuffer.UAV);

	// Run compute
	vortCS->DispatchByThreads(gridSize, gridSize, gridSize);

	// Unset resources
	vortCS->SetShaderResourceView("VelocityIn", 0);
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
	confCS->SetInt("gridSizeX", gridSize);
	confCS->SetInt("gridSizeY", gridSize);
	confCS->SetInt("gridSizeZ", gridSize);
	confCS->SetFloat("vorticityEpsilon", 1.0f); // TODO: Parameterize?
	confCS->CopyAllBufferData();

	// Set resources
	confCS->SetShaderResourceView("VorticityIn", vorticityBuffer.SRV);
	confCS->SetShaderResourceView("VelocityIn", velocityBuffers[0].SRV);
	confCS->SetUnorderedAccessView("VelocityOut", velocityBuffers[1].UAV);

	// Run compute
	confCS->DispatchByThreads(gridSize, gridSize, gridSize);

	// Unset resources
	confCS->SetShaderResourceView("VorticityIn", 0);
	confCS->SetShaderResourceView("VelocityIn", 0);
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


