#include "Renderer.h"
#include "Assets.h"
#include "SimpleShader.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"

#include "Assets.h"

#include <DirectXMath.h>
#include <algorithm>

using namespace DirectX;

 Renderer::Renderer(
	const std::vector<GameEntity*>& entities,
	const std::vector<Light>& lights,
	unsigned int activeLightCount,
	Sky* sky,
	unsigned int windowWidth,
	unsigned int windowHeight,
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain,
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV,
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV) :
		entities(entities),
		lights(lights),
		activeLightCount(activeLightCount),
		sky(sky),
		windowWidth(windowWidth),
		windowHeight(windowHeight),
		device(device),
		context(context),
		swapChain(swapChain),
		backBufferRTV(backBufferRTV),
		depthBufferDSV(depthBufferDSV),
		vsPerFrameConstantBuffer(0),
		psPerFrameConstantBuffer(0),
		pointLightsVisible(true),
		ssaoSamples(64),
		ssaoRadius(0.25f),
		ssaoEnabled(true),
		ambientNonPBR(0.1f, 0.1f, 0.25f)
{
	// Validate active light count
	activeLightCount = min(activeLightCount, MAX_LIGHTS);

	// Initialize structs
	vsPerFrameData = {};
	psPerFrameData = {};

	// Grab two shaders on which to base per-frame cbuffers
	// Note: We're assuming ALL entity/material per-frame buffers are identical!
	//       And that they're all called "perFrame"
	Assets& assets = Assets::GetInstance();
	SimplePixelShader* ps = assets.GetPixelShader("PixelShaderPBR.cso");
	SimpleVertexShader* vs = assets.GetVertexShader("VertexShader.cso");

	// Struct to hold the descriptions from existing buffers
	D3D11_BUFFER_DESC bufferDesc = {};
	const SimpleConstantBuffer* scb = 0;

	// Make a new buffer that matches the existing PS per-frame buffer
	scb = ps->GetBufferInfo("perFrame");
	scb->ConstantBuffer.Get()->GetDesc(&bufferDesc);
	device->CreateBuffer(&bufferDesc, 0, psPerFrameConstantBuffer.GetAddressOf());

	// Make a new buffer that matches the existing PS per-frame buffer
	scb = vs->GetBufferInfo("perFrame");
	scb->ConstantBuffer.Get()->GetDesc(&bufferDesc);
	device->CreateBuffer(&bufferDesc, 0, vsPerFrameConstantBuffer.GetAddressOf());
	
	// Create render targets (just calling post resize which sets them all up)
	PostResize(windowWidth, windowHeight, backBufferRTV, depthBufferDSV);

	// Set up the ssao offsets (count must match shader!)
	for (int i = 0; i < ARRAYSIZE(ssaoOffsets); i++)
	{
		ssaoOffsets[i] = XMFLOAT4(
			(float)rand() / RAND_MAX * 2 - 1,	// -1 to 1
			(float)rand() / RAND_MAX * 2 - 1,	// -1 to 1
			(float)rand() / RAND_MAX,			// 0 to 1
			0);
		
		XMVECTOR v = XMVector3Normalize(XMLoadFloat4(&ssaoOffsets[i]));

		// Scale up over the array
		float scale = (float)i / ARRAYSIZE(ssaoOffsets);
		XMVECTOR scaleVector = XMVectorLerp(
			XMVectorSet(0.1f, 0.1f, 0.1f, 1),
			XMVectorSet(1, 1, 1, 1),
			scale * scale);

		XMStoreFloat4(&ssaoOffsets[i], v * scaleVector);
		
	}

	// Set up shadow mapping resources
	CreateShadowMapResources(1024, 10.0f);
}

Renderer::~Renderer()
{

}

void Renderer::Render(Camera* camera)
{
	// Clear all targets and depth buffers
	const float color[4] = { 0, 0, 0, 1 };
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	
	// Clear shadow map
	context->ClearDepthStencilView(shadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Clear render targets
	for (auto& rt : renderTargetRTVs) context->ClearRenderTargetView(rt.Get(), color);
	const float depth[4] = { 1,0,0,0 };
	context->ClearRenderTargetView(renderTargetRTVs[SCENE_DEPTHS].Get(), depth);

	// Render the shadow map for this frame before any other rendering
	RenderShadowMap();

	// Now handle MRTs for the actual frame
	const int numTargets = 4;
	ID3D11RenderTargetView* targets[numTargets] = {};
	targets[0] = renderTargetRTVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT].Get();
	targets[1] = renderTargetRTVs[RenderTargetType::SCENE_AMBIENT].Get();
	targets[2] = renderTargetRTVs[RenderTargetType::SCENE_NORMALS].Get();
	targets[3] = renderTargetRTVs[RenderTargetType::SCENE_DEPTHS].Get();
	context->OMSetRenderTargets(numTargets, targets, depthBufferDSV.Get());

	// Collect all per-frame data and copy to GPU
	{
		// vs ----
		vsPerFrameData.ViewMatrix = camera->GetView();
		vsPerFrameData.ProjectionMatrix = camera->GetProjection();
		vsPerFrameData.ShadowViewMatrix = shadowViewMatrix;
		vsPerFrameData.ShadowProjectionMatrix = shadowProjectionMatrix;
		context->UpdateSubresource(vsPerFrameConstantBuffer.Get(), 0, 0, &vsPerFrameData, 0, 0);

		// ps ----
		memcpy(&psPerFrameData.Lights, &lights[0], sizeof(Light) * activeLightCount);
		psPerFrameData.LightCount = activeLightCount;
		psPerFrameData.CameraPosition = camera->GetTransform()->GetPosition();
		psPerFrameData.TotalSpecIBLMipLevels = sky->GetTotalSpecularIBLMipLevels();
		psPerFrameData.AmbientNonPBR = ambientNonPBR;
		context->UpdateSubresource(psPerFrameConstantBuffer.Get(), 0, 0, &psPerFrameData, 0, 0);
	}

	// Make a copy of the renderable list so we can sort it
	std::vector<GameEntity*> toDraw(entities);
	std::sort(toDraw.begin(), toDraw.end(), [](const auto& e1, const auto& e2)
		{
			// Compare pointers to materials
			return e1->GetMaterial() < e2->GetMaterial();
		});

	// Draw all of the entities
	SimpleVertexShader* currentVS = 0; 
	SimplePixelShader* currentPS = 0;
	Material* currentMaterial = 0;
	Mesh* currentMesh = 0;
	for (auto ge : toDraw)
	{
		// Track the current material and swap as necessary
		// (including swapping shaders)
		if (currentMaterial != ge->GetMaterial())
		{
			currentMaterial = ge->GetMaterial();

			// Swap vertex shader if necessary
			if (currentVS != currentMaterial->GetVS())
			{
				currentVS = currentMaterial->GetVS();
				currentVS->SetShader();

				// Must re-bind per-frame cbuffer as
				// as we're using the renderer's now!
				// Note: Would be nice to have the option
				//       for SimpleShader to NOT auto-bind
				//       cbuffers - might add this feature
				context->VSSetConstantBuffers(0, 1, vsPerFrameConstantBuffer.GetAddressOf());
			}

			// Swap pixel shader if necessary
			if (currentPS != currentMaterial->GetPS())
			{
				currentPS = currentMaterial->GetPS();
				currentPS->SetShader();

				// Must re-bind per-frame cbuffer as
				// as we're using the renderer's now!
				context->PSSetConstantBuffers(0, 1, psPerFrameConstantBuffer.GetAddressOf());

				// Set IBL textures now, too
				currentPS->SetShaderResourceView("IrradianceIBLMap", sky->GetIrradianceMap());
				currentPS->SetShaderResourceView("SpecularIBLMap", sky->GetSpecularMap());
				currentPS->SetShaderResourceView("BrdfLookUpMap", sky->GetBRDFLookUpTexture());

				// Set shadow resources
				currentPS->SetShaderResourceView("ShadowMap", shadowSRV);
				currentPS->SetSamplerState("ShadowSampler", shadowSampler);
			}

			// Now that the material is set, we should
			// copy per-material data to its cbuffers
			currentMaterial->SetPerMaterialDataAndResources(true);
		}

		// Also track current mesh
		if (currentMesh != ge->GetMesh())
		{
			currentMesh = ge->GetMesh();

			// Bind new buffers
			UINT stride = sizeof(Vertex);
			UINT offset = 0;
			context->IASetVertexBuffers(0, 1, currentMesh->GetVertexBuffer().GetAddressOf(), &stride, &offset);
			context->IASetIndexBuffer(currentMesh->GetIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
		}


		// Handle per-object data last (only VS at the moment)
		if (currentVS != 0)
		{
			Transform* trans = ge->GetTransform();
			currentVS->SetMatrix4x4("world", trans->GetWorldMatrix());
			currentVS->SetMatrix4x4("worldInverseTranspose", trans->GetWorldInverseTransposeMatrix());
			currentVS->CopyBufferData("perObject");
		}

		// Draw the entity
		if (currentMesh != 0)
		{
			context->DrawIndexed(currentMesh->GetIndexCount(), 0, 0);
		}
	}

	
	// Draw the sky after all solid objects,
	// but before transparent ones
	sky->Draw(camera);


	// Assets for following steps
	Assets& assets = Assets::GetInstance();
	SimpleVertexShader* vs = assets.GetVertexShader("FullscreenVS.cso");
	vs->SetShader();

	// Render the SSAO results
	{
		// Set up ssao render pass
		targets[0] = renderTargetRTVs[RenderTargetType::SSAO_RESULTS].Get();
		targets[1] = 0;
		targets[2] = 0;
		targets[3] = 0;
		context->OMSetRenderTargets(numTargets, targets, 0);

		SimplePixelShader* ssaoPS = assets.GetPixelShader("SsaoPS.cso");
		ssaoPS->SetShader();

		// Calculate the inverse of the camera matrices
		XMFLOAT4X4 invView, invProj, view = camera->GetView(), proj = camera->GetProjection();
		XMStoreFloat4x4(&invView, XMMatrixInverse(0, XMLoadFloat4x4(&view)));
		XMStoreFloat4x4(&invProj, XMMatrixInverse(0, XMLoadFloat4x4(&proj)));
		ssaoPS->SetMatrix4x4("invViewMatrix", invView);
		ssaoPS->SetMatrix4x4("invProjMatrix", invProj);
		ssaoPS->SetMatrix4x4("viewMatrix", view);
		ssaoPS->SetMatrix4x4("projectionMatrix", proj);
		ssaoPS->SetData("offsets", ssaoOffsets, sizeof(XMFLOAT4) * ARRAYSIZE(ssaoOffsets));
		ssaoPS->SetFloat("ssaoRadius", ssaoRadius);
		ssaoPS->SetInt("ssaoSamples", ssaoSamples);
		ssaoPS->SetFloat2("randomTextureScreenScale", XMFLOAT2(windowWidth / 4.0f, windowHeight / 4.0f));
		ssaoPS->CopyAllBufferData();

		ssaoPS->SetShaderResourceView("Normals", renderTargetSRVs[RenderTargetType::SCENE_NORMALS]);
		ssaoPS->SetShaderResourceView("Depths", renderTargetSRVs[RenderTargetType::SCENE_DEPTHS]);
		ssaoPS->SetShaderResourceView("Random", assets.GetTexture("random"));

		context->Draw(3, 0);
	}


	// SSAO Blur step
	{
		// Set up blur (assuming all other targets are null here)
		targets[0] = renderTargetRTVs[RenderTargetType::SSAO_BLUR].Get();
		context->OMSetRenderTargets(1, targets, 0);

		SimplePixelShader* ps = assets.GetPixelShader("SsaoBlurPS.cso");
		ps->SetShader();
		ps->SetShaderResourceView("SSAO", renderTargetSRVs[RenderTargetType::SSAO_RESULTS]);
		ps->SetFloat2("pixelSize", XMFLOAT2(1.0f / windowWidth, 1.0f / windowHeight));
		ps->CopyAllBufferData();
		context->Draw(3, 0);
	}

	// Final combine
	{
		// Re-enable back buffer (assuming all other targets are null here)
		targets[0] = backBufferRTV.Get();
		context->OMSetRenderTargets(1, targets, 0);

		SimplePixelShader* ps = assets.GetPixelShader("SsaoCombinePS.cso");
		ps->SetShader();
		ps->SetShaderResourceView("SceneColorsNoAmbient", renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT]);
		ps->SetShaderResourceView("Ambient", renderTargetSRVs[RenderTargetType::SCENE_AMBIENT]);
		ps->SetShaderResourceView("SSAOBlur", renderTargetSRVs[RenderTargetType::SSAO_BLUR]);
		ps->SetInt("ssaoEnabled", ssaoEnabled);
		ps->SetInt("ssaoOutputOnly", ssaoOutputOnly);
		ps->SetFloat2("pixelSize", XMFLOAT2(1.0f / windowWidth, 1.0f / windowHeight));
		ps->CopyAllBufferData();
		context->Draw(3, 0);
	}

	// Draw the lights if necessary
	if (pointLightsVisible)
	{
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
		DrawPointLights(camera);
	}

	// Draw IMGUI
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Present and re-bind the RTV
	swapChain->Present(0, 0);
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());

	// Unbind all SRVs at the end of the frame so they're not still bound for input
	// when we begin the MRTs of the next frame
	ID3D11ShaderResourceView* nullSRVs[16] = {};
	context->PSSetShaderResources(0, 16, nullSRVs);
}

void Renderer::PreResize()
{
	backBufferRTV.Reset();
	depthBufferDSV.Reset();
}

void Renderer::PostResize(
	unsigned int windowWidth,
	unsigned int windowHeight,
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, 
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV)
{
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->backBufferRTV = backBufferRTV;
	this->depthBufferDSV = depthBufferDSV;

	// Release all of the renderer-specific render targets
	for (auto& rt : renderTargetSRVs) rt.Reset();
	for (auto& rt : renderTargetRTVs) rt.Reset();

	// Recreate using the new window size
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT], renderTargetSRVs[RenderTargetType::SCENE_COLORS_NO_AMBIENT]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_AMBIENT], renderTargetSRVs[RenderTargetType::SCENE_AMBIENT]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_NORMALS], renderTargetSRVs[RenderTargetType::SCENE_NORMALS]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_DEPTHS], renderTargetSRVs[RenderTargetType::SCENE_DEPTHS], DXGI_FORMAT_R32_FLOAT);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_RESULTS], renderTargetSRVs[RenderTargetType::SSAO_RESULTS]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_BLUR], renderTargetSRVs[RenderTargetType::SSAO_BLUR]);
}

unsigned int Renderer::GetActiveLightCount() { return activeLightCount; }
void Renderer::SetActiveLightCount(unsigned int count){	activeLightCount = min(count, MAX_LIGHTS); }

void Renderer::SetPointLightsVisible(bool visible) { pointLightsVisible = visible; }
bool Renderer::GetPointLightsVisible() { return pointLightsVisible; }

void Renderer::SetSSAOEnabled(bool enabled) { ssaoEnabled = enabled; }
bool Renderer::GetSSAOEnabled() { return ssaoEnabled; }

void Renderer::SetSSAORadius(float radius) { ssaoRadius = radius; }
float Renderer::GetSSAORadius() { return ssaoRadius; }

void Renderer::SetSSAOSamples(int samples) { ssaoSamples = max(0, min(samples, ARRAYSIZE(ssaoOffsets))); }
int Renderer::GetSSAOSamples() { return ssaoSamples; }

void Renderer::SetSSAOOutputOnly(bool ssaoOnly) { ssaoOutputOnly = ssaoOnly; }
bool Renderer::GetSSAOOutputOnly() { return ssaoOutputOnly; }

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetRenderTargetSRV(RenderTargetType type)
{ 
	if (type < 0 || type >= RenderTargetType::RENDER_TARGET_TYPE_COUNT)
		return 0;

	return renderTargetSRVs[type];
}

void Renderer::CreateShadowMapResources(unsigned int shadowMapSize, float projectionSize)
{
	// Create the initial shadow map
	ResizeShadowMap(shadowMapSize);

	// Create the special "comparison" sampler state for shadows
	D3D11_SAMPLER_DESC shadowSampDesc = {};
	shadowSampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // COMPARISON filter!
	shadowSampDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
	shadowSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.BorderColor[0] = 1.0f;
	shadowSampDesc.BorderColor[1] = 1.0f;
	shadowSampDesc.BorderColor[2] = 1.0f;
	shadowSampDesc.BorderColor[3] = 1.0f;
	device->CreateSamplerState(&shadowSampDesc, &shadowSampler);

	// Create a rasterizer state
	D3D11_RASTERIZER_DESC shadowRastDesc = {};
	shadowRastDesc.FillMode = D3D11_FILL_SOLID;
	shadowRastDesc.CullMode = D3D11_CULL_BACK;
	shadowRastDesc.DepthClipEnable = true;
	shadowRastDesc.DepthBias = 1000; // Multiplied by (smallest possible positive value storable in the depth buffer)
	shadowRastDesc.DepthBiasClamp = 0.0f;
	shadowRastDesc.SlopeScaledDepthBias = 1.0f;
	device->CreateRasterizerState(&shadowRastDesc, &shadowRasterizer);

	// Create the "camera" matrices for the shadow map rendering

	// View matrix will be re-created each frame in the event the light rotates
	
	// Projection
	UpdateShadowProjection(projectionSize);
}


void Renderer::UpdateShadowProjection(float projectionSize)
{
	shadowProjectionSize = projectionSize;

	// Projection - we want ORTHOGRAPHIC for directional light shadows
	// NOTE: This particular projection is set up to be SMALLER than
	// the overall "scene", to show what happens when objects go
	// outside the shadow area.  In a game, you'd never want the
	// user to see this edge, but I'm specifically making the projection
	// small in this demo to show you that it CAN happen.
	//
	// Ideally, the first two parameters below would be adjusted to
	// fit the scene (or however much of the scene the user can see
	// at a time).  More advanced techniques, like cascaded shadow maps,
	// would use multiple (usually 4) shadow maps with increasingly larger
	// projections to ensure large open world games have shadows "everywhere"
	XMMATRIX shProj = XMMatrixOrthographicLH(shadowProjectionSize, shadowProjectionSize, 0.1f, 100.0f);
	XMStoreFloat4x4(&shadowProjectionMatrix, shProj);
}


void Renderer::UpdateShadowView(const Light* light)
{
	XMMATRIX shView = XMMatrixLookToLH(
		XMLoadFloat3(&light->Direction) * -20,//XMVectorSet(0, 20, -20, 0),
		XMLoadFloat3(&light->Direction), //XMVectorSet(0, 0, 0, 0),
		XMVectorSet(0, 1, 0, 0));
	XMStoreFloat4x4(&shadowViewMatrix, shView);
}



void Renderer::ResizeShadowMap(unsigned int shadowMapSize)
{
	// Reset com ptrs
	shadowSRV.Reset();
	shadowDSV.Reset();

	// Save resolution
	shadowMapResolution = shadowMapSize;

	// Create the actual texture that will be the shadow map
	D3D11_TEXTURE2D_DESC shadowDesc = {};
	shadowDesc.Width = shadowMapResolution;
	shadowDesc.Height = shadowMapResolution;
	shadowDesc.ArraySize = 1;
	shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	shadowDesc.CPUAccessFlags = 0;
	shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	shadowDesc.MipLevels = 1;
	shadowDesc.MiscFlags = 0;
	shadowDesc.SampleDesc.Count = 1;
	shadowDesc.SampleDesc.Quality = 0;
	shadowDesc.Usage = D3D11_USAGE_DEFAULT;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTexture;
	device->CreateTexture2D(&shadowDesc, 0, shadowTexture.GetAddressOf());

	// Create the depth/stencil
	D3D11_DEPTH_STENCIL_VIEW_DESC shadowDSDesc = {};
	shadowDSDesc.Format = DXGI_FORMAT_D32_FLOAT;
	shadowDSDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	shadowDSDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(shadowTexture.Get(), &shadowDSDesc, shadowDSV.GetAddressOf());

	// Create the SRV for the shadow map
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	device->CreateShaderResourceView(shadowTexture.Get(), &srvDesc, shadowSRV.GetAddressOf());
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetShadowMapSRV() { return shadowSRV; }
unsigned int Renderer::GetShadowMapResolution() { return shadowMapResolution; }
float Renderer::GetShadowProjectionSize() { return shadowProjectionSize; }

void Renderer::SetShadowMapResolution(unsigned int resolution) { ResizeShadowMap(resolution); }
void Renderer::SetShadowProjectionSize(float projectionSize) { UpdateShadowProjection(projectionSize); }


void Renderer::RenderShadowMap()
{
	// Update the shadow view matrix to match the first directional light
	UpdateShadowView(&lights[0]);

	// Initial pipeline setup - No RTV necessary - Clear shadow map
	context->OMSetRenderTargets(0, 0, shadowDSV.Get());
	context->ClearDepthStencilView(shadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	context->RSSetState(shadowRasterizer.Get());

	// Need to create a viewport that matches the shadow map resolution
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)shadowMapResolution;
	viewport.Height = (float)shadowMapResolution;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// Turn on our shadow map Vertex Shader
	// and turn OFF the pixel shader entirely
	SimpleVertexShader* shadowVS = Assets::GetInstance().GetVertexShader("ShadowVS.cso");
	shadowVS->SetShader();
	shadowVS->SetMatrix4x4("view", shadowViewMatrix);
	shadowVS->SetMatrix4x4("projection", shadowProjectionMatrix);
	shadowVS->CopyBufferData("perFrame");
	context->PSSetShader(0, 0, 0); // No PS

	// Loop and draw all entities
	for (auto& e : entities)
	{
		shadowVS->SetMatrix4x4("world", e->GetTransform()->GetWorldMatrix());
		shadowVS->CopyBufferData("perObject");

		// Draw the mesh
		e->GetMesh()->SetBuffersAndDraw(context);
	}

	// After rendering the shadow map, go back to the screen
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
	viewport.Width = (float)this->windowWidth;
	viewport.Height = (float)this->windowHeight;
	context->RSSetViewports(1, &viewport);
	context->RSSetState(0);
}

void Renderer::DrawPointLights(Camera* camera)
{
	// Grab shaders
	Assets& assets = Assets::GetInstance();
	SimpleVertexShader* lightVS = assets.GetVertexShader("VertexShader.cso");
	SimplePixelShader* lightPS = assets.GetPixelShader("SolidColorPS.cso");
	Mesh* lightMesh = assets.GetMesh("Models\\sphere.obj");

	// Turn on these shaders
	lightVS->SetShader();
	lightPS->SetShader();

	// Set up vertex shader
	lightVS->SetMatrix4x4("view", camera->GetView());
	lightVS->SetMatrix4x4("projection", camera->GetProjection());

	for (unsigned int i = 0; i < activeLightCount; i++)
	{
		Light light = lights[i];

		// Only drawing points, so skip others
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Calc quick scale based on range
		// (assuming range is between 5 - 10)
		float scale = light.Range / 10.0f;

		// Make the transform for this light
		XMMATRIX rotMat = XMMatrixIdentity();
		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);
		XMMATRIX worldMat = scaleMat * rotMat * transMat;

		XMFLOAT4X4 world;
		XMFLOAT4X4 worldInvTrans;
		XMStoreFloat4x4(&world, worldMat);
		XMStoreFloat4x4(&worldInvTrans, XMMatrixInverse(0, XMMatrixTranspose(worldMat)));

		// Set up the world matrix for this light
		lightVS->SetMatrix4x4("world", world);
		lightVS->SetMatrix4x4("worldInverseTranspose", worldInvTrans);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		lightPS->SetFloat3("Color", finalColor);

		// Copy data
		lightVS->CopyAllBufferData();
		lightPS->CopyAllBufferData();

		// Draw
		lightMesh->SetBuffersAndDraw(context);
	}
}

void Renderer::CreateRenderTarget(
	unsigned int width, 
	unsigned int height, 
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv, 
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
	DXGI_FORMAT colorFormat)
{
	// Make the texture
	Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTexture;
	
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width            = width;
	texDesc.Height           = height;
	texDesc.ArraySize        = 1;
	texDesc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format           = colorFormat; 
	texDesc.MipLevels        = 1; // Usually no mip chain needed for render targets
	texDesc.MiscFlags        = 0;
	texDesc.SampleDesc.Count = 1; // Can't be zero
	device->CreateTexture2D(&texDesc, 0, rtTexture.GetAddressOf());

	// Make the render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDesc.Texture2D.MipSlice = 0;                             // Which mip are we rendering into?
	rtvDesc.Format             = texDesc.Format;                // Same format as texture
	device->CreateRenderTargetView(rtTexture.Get(), &rtvDesc, rtv.GetAddressOf());

	// Create the shader resource view using default options 
	device->CreateShaderResourceView(
		rtTexture.Get(),     // Texture resource itself
		0,                   // Null description = default SRV options
		srv.GetAddressOf()); // ComPtr<ID3D11ShaderResourceView>
}
