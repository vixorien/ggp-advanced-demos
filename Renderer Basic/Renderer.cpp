#include "Renderer.h"
#include "Assets.h"
#include "SimpleShader.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"

#include <DirectXMath.h>

using namespace DirectX;


Renderer::Renderer(
	const std::vector<GameEntity*>& entities,
	const std::vector<Light>& lights,
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
		sky(sky),
		windowWidth(windowWidth),
		windowHeight(windowHeight),
		device(device),
		context(context),
		swapChain(swapChain),
		backBufferRTV(backBufferRTV),
		depthBufferDSV(depthBufferDSV)
{
}

Renderer::~Renderer()
{

}

void Renderer::Render(Camera* camera, int lightCount_RemoveMeLater)
{
	// Clear all targets and depth buffers
	const float color[4] = { 0, 0, 0, 1 };
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(depthBufferDSV.Get(),	D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Draw all of the entities
	for (auto ge : entities)
	{
		// Set the "per frame" data
		// Note that this should literally be set once PER FRAME, before
		// the draw loop, but we're currently setting it per entity since 
		// we are just using whichever shader the current entity has.  
		// Inefficient!!!
		SimplePixelShader* ps = ge->GetMaterial()->GetPS();
		ps->SetData("Lights", (void*)(&lights[0]), sizeof(Light) * lightCount_RemoveMeLater);
		ps->SetInt("LightCount", lightCount_RemoveMeLater);
		ps->SetFloat3("CameraPosition", camera->GetTransform()->GetPosition());
		ps->CopyBufferData("perFrame");

		// Draw the entity
		ge->Draw(context, camera);
	}

	// Draw the lights. TODO: Make this toggle-able
	DrawPointLights(camera, lightCount_RemoveMeLater);

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
}


void Renderer::DrawPointLights(Camera* camera, int lightCount_RemoveMeLater)
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

	for (int i = 0; i < lightCount_RemoveMeLater; i++)
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
