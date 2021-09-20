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
		psPerFrameConstantBuffer(0)
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
				currentPS->SetShaderResourceView("BRDFMap", sky->GetBRDFLookUpTexture());
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

	// Draw the lights. TODO: Make this toggle-able
	DrawPointLights(camera);

	// Draw the sky after all solid objects,
	// but before transparent ones
	sky->Draw(camera);

	// Draw IMGUI
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Present and re-bind the RTV
	swapChain->Present(0, 0);
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
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
}

unsigned int Renderer::GetActiveLightCount()
{
	return activeLightCount;
}

void Renderer::SetActiveLightCount(unsigned int count)
{
	activeLightCount = min(count, MAX_LIGHTS);
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
