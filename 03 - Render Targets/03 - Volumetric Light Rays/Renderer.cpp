#include "Renderer.h"
#include "Assets.h"

#include "../../Common/ImGui/imgui.h"
#include "../../Common/ImGui/imgui_impl_dx11.h"

using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

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
	psPerFrameData{},
	indirectLighting(true),
	iblIntensity(1.0f),
	lightRaySamples(128),
	lightRayDensity(1.0f),
	lightRaySampleWeight(0.2f),
	lightRayDecay(0.98f),
	lightRayExposure(0.2f),
	lightRaySunDirection(0, 0, 1),
	lightRaySunFalloffExponent(128.0f),
	lightRaySunColor(1, 1, 1),
	lightRayUseSkyboxColor(false)
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

	// Call PostResize() once to trigger render target creation
	PostResize(windowWidth, windowHeight, backBufferRTV, depthBufferDSV);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetRenderTargetSRV(RenderTargetType type)
{
	return renderTargetSRVs[type];
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

	// Reset all render targets
	for (auto& rt : renderTargetSRVs) rt.Reset();
	for (auto& rt : renderTargetRTVs) rt.Reset();

	// Recreate using the new window size
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_COLORS], renderTargetSRVs[RenderTargetType::SCENE_COLORS]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SUN_AND_OCCLUDERS], renderTargetSRVs[RenderTargetType::SUN_AND_OCCLUDERS]);
}

void Renderer::FrameStart()
{
	// Clear the back buffer (erases what's on the screen)
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	context->ClearRenderTargetView(backBufferRTV.Get(), clearColor);

	// Clear the depth buffer (resets per-pixel occlusion information)
	context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Clear all targets
	const float black[4] = { 0,0,0,0 };
	for (auto& rt : renderTargetRTVs) context->ClearRenderTargetView(rt.Get(), black);

	// Set render targets for initial render pass
	const int numTargets = 4;
	ID3D11RenderTargetView* targets[numTargets] = {};
	targets[0] = renderTargetRTVs[RenderTargetType::SCENE_COLORS].Get();
	targets[1] = renderTargetRTVs[RenderTargetType::SUN_AND_OCCLUDERS].Get();
	context->OMSetRenderTargets(numTargets, targets, depthBufferDSV.Get());
}


void Renderer::FrameEnd(bool vsync, std::shared_ptr<Camera> camera)
{
	// Handle post processing now that the main render is done
	PostProcess(camera);

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


void Renderer::PostProcess(std::shared_ptr<Camera> camera)
{
	// Unbind all potential render targets before post process
	const int numTargets = 4;
	ID3D11RenderTargetView* targets[numTargets] = {};
	context->OMSetRenderTargets(numTargets, targets, 0);

	// Set up the vertex shader for post processing
	Assets& assets = Assets::GetInstance();
	std::shared_ptr<SimpleVertexShader> vs = assets.GetVertexShader(L"FullscreenVS");
	vs->SetShader();

	// Raymarch for volumetric light as the last step
	{
		// Re-enable back buffer (assuming all other targets are null here)
		targets[0] = backBufferRTV.Get();
		context->OMSetRenderTargets(1, targets, 0);

		// Determine light's current screen space position
		XMFLOAT4X4 view = camera->GetView();
		XMFLOAT4X4 proj = camera->GetProjection();
		XMVECTOR lightPosWorld = XMLoadFloat3(&lightRaySunDirection);
		XMVECTOR lightPosScreenVec = XMVector4Transform(lightPosWorld,
			XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj));
		lightPosScreenVec /= XMVectorGetW(lightPosScreenVec); // Perspective divide

		XMFLOAT2 lightPosScreen;
		XMStoreFloat2(&lightPosScreen, lightPosScreenVec);

		// Draw
		std::shared_ptr<SimplePixelShader> ps = assets.GetPixelShader(L"LightRayPS");
		ps->SetShader();
		ps->SetShaderResourceView("SceneColors", renderTargetSRVs[RenderTargetType::SCENE_COLORS]);
		ps->SetShaderResourceView("SunAndOccluders", renderTargetSRVs[RenderTargetType::SUN_AND_OCCLUDERS]);
		ps->SetInt("numSamples", lightRaySamples);
		ps->SetFloat("density", lightRayDensity);
		ps->SetFloat("weight", lightRaySampleWeight);
		ps->SetFloat("decay", lightRayDecay);
		ps->SetFloat("exposure", lightRayExposure);
		ps->SetFloat2("lightPosScreenSpace", lightPosScreen);
		ps->CopyAllBufferData();
		context->Draw(3, 0);
	}

	// Restore back buffer now that post processing is complete
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

	// Unbind all SRVs at the end of the frame so they're not still bound for input
	// when we begin the MRTs of the next frame
	ID3D11ShaderResourceView* nullSRVs[128] = {};
	context->PSSetShaderResources(0, 128, nullSRVs);
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
		ps->SetInt("specularIBLTotalMipLevels", scene->GetSky()->GetTotalSpecularIBLMipLevels());
		ps->SetInt("indirectLightingEnabled", indirectLighting);
		ps->SetFloat("iblIntensity", iblIntensity);
		ps->CopyBufferData("perFrame");

		// Set IBL textures now, too
		ps->SetShaderResourceView("IrradianceIBLMap", scene->GetSky()->GetIrradianceMap());
		ps->SetShaderResourceView("SpecularIBLMap", scene->GetSky()->GetSpecularMap());
		ps->SetShaderResourceView("BrdfLookUpMap", scene->GetSky()->GetBRDFLookUpTexture());
		ps->SetSamplerState("ClampSampler", Assets::GetInstance().GetSampler(L"Samplers/anisotropic16Clamp"));

		// Draw the entity
		ge->Draw(context, scene->GetCurrentCamera());
	}

	// Draw the sky
	// Note: Injecting some light ray details here
	std::shared_ptr<SimplePixelShader> skyPS = Assets::GetInstance().GetPixelShader(L"SkyPS");
	skyPS->SetFloat3("sunDirection", lightRaySunDirection);
	skyPS->SetFloat("falloffExponent", lightRaySunFalloffExponent);
	skyPS->SetFloat3("sunColor", lightRaySunColor);
	skyPS->SetInt("useSkyboxColor", (int)lightRayUseSkyboxColor);
	skyPS->CopyAllBufferData();
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
		psPerFrameData.TotalSpecIBLMipLevels = scene->GetSky()->GetTotalSpecularIBLMipLevels();
		psPerFrameData.IndirectLightingEnabled = indirectLighting;
		psPerFrameData.IBLIntensity = iblIntensity;

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

				// Set IBL textures now, too
				currentPS->SetShaderResourceView("IrradianceIBLMap", scene->GetSky()->GetIrradianceMap());
				currentPS->SetShaderResourceView("SpecularIBLMap", scene->GetSky()->GetSpecularMap());
				currentPS->SetShaderResourceView("BrdfLookUpMap", scene->GetSky()->GetBRDFLookUpTexture());
				currentPS->SetSamplerState("ClampSampler", Assets::GetInstance().GetSampler(L"Samplers/anisotropic16Clamp"));
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
	// Note: Injecting some light ray details here
	std::shared_ptr<SimplePixelShader> skyPS = Assets::GetInstance().GetPixelShader(L"SkyPS");
	skyPS->SetFloat3("sunDirection", lightRaySunDirection);
	skyPS->SetFloat("falloffExponent", lightRaySunFalloffExponent);
	skyPS->SetFloat3("sunColor", lightRaySunColor);
	skyPS->SetInt("useSkyboxColor", (int)lightRayUseSkyboxColor);
	skyPS->CopyAllBufferData();
	scene->GetSky()->Draw(scene->GetCurrentCamera());
}



void Renderer::CreateRenderTarget(unsigned int width, unsigned int height, Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, DXGI_FORMAT colorFormat)
{
	// Make the texture
	Microsoft::WRL::ComPtr<ID3D11Texture2D> rtTexture;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Need both!
	texDesc.Format = colorFormat;
	texDesc.MipLevels = 1; // Usually no mip chain needed for render targets
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1; // Can't be zero
	device->CreateTexture2D(&texDesc, 0, rtTexture.GetAddressOf());

	// Make the render target view
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // This points to a Texture2D
	rtvDesc.Texture2D.MipSlice = 0;		// Which mip are we rendering into?
	rtvDesc.Format = texDesc.Format;	// Same format as texture
	device->CreateRenderTargetView(rtTexture.Get(), &rtvDesc, rtv.GetAddressOf());

	// Create the shader resource view using default options 
	device->CreateShaderResourceView(
		rtTexture.Get(),     // Texture resource itself
		0,                   // Null description = default SRV options
		srv.GetAddressOf()); // ComPtr<ID3D11ShaderResourceView>
}


