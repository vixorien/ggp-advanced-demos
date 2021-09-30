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
		pointLightsVisible(true)
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
	
	// Create render targets
	CreateRenderTarget(windowWidth, windowHeight, sceneColorsRTV, sceneColorsSRV);
	CreateRenderTarget(windowWidth, windowHeight, sceneNormalsRTV, sceneNormalsSRV);
}

Renderer::~Renderer()
{

}

void Renderer::Render(Camera* camera)
{
	// Clear all targets and depth buffers
	const float color[4] = { 0, 0, 0, 1 };
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearRenderTargetView(sceneColorsRTV.Get(), color);
	context->ClearRenderTargetView(sceneNormalsRTV.Get(), color);
	context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	ID3D11RenderTargetView* targets[2] = {};
	targets[0] = sceneColorsRTV.Get();
	targets[1] = sceneNormalsRTV.Get();
	context->OMSetRenderTargets(2, targets, depthBufferDSV.Get());

	// Collect all per-frame data and copy to GPU
	{
		// vs ----
		vsPerFrameData.ViewMatrix = camera->GetView();
		vsPerFrameData.ProjectionMatrix = camera->GetProjection();
		context->UpdateSubresource(vsPerFrameConstantBuffer.Get(), 0, 0, &vsPerFrameData, 0, 0);

		// ps ----
		memcpy(&psPerFrameData.Lights, &lights[0], sizeof(Light) * activeLightCount);
		psPerFrameData.LightCount = activeLightCount;
		psPerFrameData.CameraPosition = camera->GetTransform()->GetPosition();
		psPerFrameData.TotalSpecIBLMipLevels = sky->GetTotalSpecularIBLMipLevels();
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

	// Draw the lights if necessary
	if(pointLightsVisible)
		DrawPointLights(camera);

	// Draw the sky after all solid objects,
	// but before transparent ones
	sky->Draw(camera);


	// Re-enable back buffer
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

	// Lastly, get the final color results to the screen!
	Assets& assets = Assets::GetInstance();
	SimpleVertexShader* vs = assets.GetVertexShader("FullscreenVS.cso");
	SimplePixelShader* ps = assets.GetPixelShader("SimpleTexturePS.cso");
	vs->SetShader();
	ps->SetShader();
	ps->SetShaderResourceView("Pixels", sceneColorsSRV);
	// Note: not setting a sampler here for 2 reasons:
	// - 1: We don't have a sampler created here in the renderer
	// - 2: A null sampler just uses the default sampling options, which
	//      are good enough for just plastering a texture on the screen
	context->Draw(3, 0);

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
	sceneColorsRTV.Reset();
	sceneNormalsRTV.Reset();
	sceneColorsSRV.Reset();
	sceneNormalsSRV.Reset();

	// Recreate using the new window size
	CreateRenderTarget(windowWidth, windowHeight, sceneColorsRTV, sceneColorsSRV);
	CreateRenderTarget(windowWidth, windowHeight, sceneNormalsRTV, sceneNormalsSRV);
}

unsigned int Renderer::GetActiveLightCount()
{
	return activeLightCount;
}

void Renderer::SetActiveLightCount(unsigned int count)
{
	activeLightCount = min(count, MAX_LIGHTS);
}

void Renderer::SetPointLightsVisible(bool visible)
{
	pointLightsVisible = visible;
}

bool Renderer::GetPointLightsVisible()
{
	return pointLightsVisible;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetSceneColorsSRV() { return sceneColorsSRV; }
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetSceneNormalsSRV() { return sceneNormalsSRV; }

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
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
{
	// Make the texture
	Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTexture;
	
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width            = width;
	texDesc.Height           = height;
	texDesc.ArraySize        = 1;
	texDesc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM; // Might occasionally use other formats
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
