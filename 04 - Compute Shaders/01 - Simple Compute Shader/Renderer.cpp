#include "Renderer.h"

#include "../../Common/ImGui/imgui.h"
#include "../../Common/ImGui/imgui_impl_dx11.h"

Renderer::Renderer(
	Microsoft::WRL::ComPtr<ID3D11Device> device, 
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain, 
	unsigned int windowWidth, 
	unsigned int windowHeight, 
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferRTV, 
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthBufferDSV) 
	:
	device(device),
	context(context),
	swapChain(swapChain),
	windowWidth(windowWidth),
	windowHeight(windowHeight),
	backBufferRTV(backBufferRTV),
	depthBufferDSV(depthBufferDSV),
	vsPerFrameData{},
	psPerFrameData{}
{
	// Create per-frame constant buffers for the renderer
	D3D11_BUFFER_DESC perFrame = {};
	perFrame.Usage = D3D11_USAGE_DYNAMIC;
	perFrame.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	perFrame.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	
	// VS
	perFrame.ByteWidth = (sizeof(VSPerFrameData) + 15) / 16 * 16; // Align to 16 bytes
	device->CreateBuffer(&perFrame, 0, vsPerFrameConstantBuffer.GetAddressOf());

	// PS
	perFrame.ByteWidth = (sizeof(PSPerFrameData) + 15) / 16 * 16; // Align to 16 bytes
	device->CreateBuffer(&perFrame, 0, psPerFrameConstantBuffer.GetAddressOf());


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

void Renderer::FrameStart()
{
	// Clear the back buffer (erases what's on the screen)
	const float bgColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black
	context->ClearRenderTargetView(backBufferRTV.Get(), bgColor);

	// Clear the depth buffer (resets per-pixel occlusion information)
	context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Renderer::FrameEnd(bool vsync)
{
	// Draw the UI after everything else
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Present the back buffer to the user
	swapChain->Present(
		vsync ? 1 : 0,
		vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING);

	// Must re-bind buffers after presenting, as they become unbound
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
}


void Renderer::RenderSimple(std::shared_ptr<Scene> scene, unsigned int activeLightCount)
{
	// Draw entities
	for (auto& ge : scene->GetEntities())
	{
		// Set the "per frame" data
		// Note that this should literally be set once PER FRAME, before
		// the draw loop, but we're currently setting it per entity since 
		// we are just using whichever shader the current entity has.  
		// Inefficient!!!
		std::shared_ptr<SimplePixelShader> ps = ge->GetMaterial()->GetPixelShader();
		ps->SetData("lights", (void*)(&scene->GetLights()[0]), sizeof(Light) * (unsigned int)scene->GetLights().size());
		ps->SetInt("lightCount", activeLightCount);
		ps->SetFloat3("cameraPosition", scene->GetCurrentCamera()->GetTransform()->GetPosition());
		ps->CopyBufferData("perFrame");

		// Draw the entity
		ge->Draw(context, scene->GetCurrentCamera());
	}

	// Draw the sky
	scene->GetSky()->Draw(scene->GetCurrentCamera());
}


void Renderer::RenderOptimized(std::shared_ptr<Scene> scene, unsigned int activeLightCount)
{
	// Collect all per-frame data and copy to GPU
	{
		std::shared_ptr<Camera> camera = scene->GetCurrentCamera();
		D3D11_MAPPED_SUBRESOURCE map = {};

		// vs ----
		vsPerFrameData.ViewMatrix = camera->GetView();
		vsPerFrameData.ProjectionMatrix = camera->GetProjection();

		context->Map(vsPerFrameConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, &vsPerFrameData, sizeof(VSPerFrameData));
		context->Unmap(vsPerFrameConstantBuffer.Get(), 0);

		// ps ----
		memcpy(&psPerFrameData.Lights, &scene->GetLights()[0], sizeof(Light) * activeLightCount);
		psPerFrameData.LightCount = activeLightCount;
		psPerFrameData.CameraPosition = camera->GetTransform()->GetPosition();

		context->Map(psPerFrameConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, &psPerFrameData, sizeof(PSPerFrameData));
		context->Unmap(psPerFrameConstantBuffer.Get(), 0);
	}

	// Make a copy of the renderable list so we can sort it
	std::vector<std::shared_ptr<GameEntity>> toDraw(scene->GetEntities());
	std::sort(toDraw.begin(), toDraw.end(), [](const auto& e1, const auto& e2)
		{
			// Compare pointers to materials
			return e1->GetMaterial() < e2->GetMaterial();
		});


	// Draw all of the entities
	std::shared_ptr<SimpleVertexShader> currentVS = 0;
	std::shared_ptr<SimplePixelShader> currentPS = 0;
	std::shared_ptr<Material> currentMaterial = 0;
	std::shared_ptr<Mesh> currentMesh = 0;
	for (auto& ge : toDraw)
	{
		// Track the current material and swap as necessary
		// (including swapping shaders)
		if (currentMaterial != ge->GetMaterial())
		{
			currentMaterial = ge->GetMaterial();

			// Swap vertex shader if necessary
			if (currentVS != currentMaterial->GetVertexShader())
			{
				currentVS = currentMaterial->GetVertexShader();
				currentVS->SetShader();

				// Must re-bind per-frame cbuffer as
				// as we're using the renderer's now!
				// Note: Would be nice to have the option
				//       for SimpleShader to NOT auto-bind
				//       cbuffers - might add this feature
				context->VSSetConstantBuffers(0, 1, vsPerFrameConstantBuffer.GetAddressOf());
			}

			// Swap pixel shader if necessary
			if (currentPS != currentMaterial->GetPixelShader())
			{
				currentPS = currentMaterial->GetPixelShader();
				currentPS->SetShader();

				// Must re-bind per-frame cbuffer as
				// as we're using the renderer's now!
				context->PSSetConstantBuffers(0, 1, psPerFrameConstantBuffer.GetAddressOf());
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

	// Draw the sky
	scene->GetSky()->Draw(scene->GetCurrentCamera());
}


