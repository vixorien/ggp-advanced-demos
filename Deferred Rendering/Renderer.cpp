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
		pointLightsVisible(false),
		ssaoSamples(64),
		ssaoRadius(0.25f),
		ssaoEnabled(true),
		ambientNonPBR(0.1f, 0.1f, 0.25f),
	    iblIntensity(1.0f),
		vsPerFrameData(0),
		psPerFrameData(0)
{
	// Validate active light count
	activeLightCount = min(activeLightCount, MAX_LIGHTS);

	// Grab two shaders on which to base per-frame cbuffers
	// Note: We're assuming ALL entity/material per-frame buffers are identical!
	//       And that they're all called "perFrame"
	Assets& assets = Assets::GetInstance();
	SimplePixelShader* ps = assets.GetPixelShader("PixelShaderPBR.cso");
	SimplePixelShader* psDeferred = assets.GetPixelShader("GBufferRenderPS.cso");
	SimpleVertexShader* vs = assets.GetVertexShader("VertexShader.cso");

	// Create per frame data structs
	// (On the heap because the PS one
	// takes up WAY too much room for the stack)
	vsPerFrameData = new VSPerFrameData();
	psPerFrameData = new PSPerFrameData();

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

	// Set up states for deferred rendering

	// Additive blending state for acculumating deferred lights
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = false;
		blendDesc.IndependentBlendEnable = false;
		blendDesc.RenderTarget[0].BlendEnable = true;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		device->CreateBlendState(&blendDesc, deferredAdditiveBlendState.GetAddressOf());
	}

	// Inside out rasterizer state
	{
		D3D11_RASTERIZER_DESC rDesc = {};
		rDesc.DepthClipEnable = true;
		rDesc.CullMode = D3D11_CULL_FRONT;
		rDesc.FillMode = D3D11_FILL_SOLID;
		device->CreateRasterizerState(&rDesc, deferredCullFrontRasterState.GetAddressOf());
	}

	// Depth states for deferred lights
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};

		dsDesc.DepthEnable = true;
		dsDesc.DepthFunc = D3D11_COMPARISON_GREATER;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		device->CreateDepthStencilState(&dsDesc, deferredDirectionalLightDepthState.GetAddressOf());

		dsDesc.DepthEnable = true;
		dsDesc.DepthFunc = D3D11_COMPARISON_GREATER;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		device->CreateDepthStencilState(&dsDesc, deferredPointLightDepthState.GetAddressOf());
	}

}

Renderer::~Renderer()
{
	delete vsPerFrameData;
	delete psPerFrameData;
}

void Renderer::Render(Camera* camera)
{
	// Will need some assets throughout
	Assets& assets = Assets::GetInstance();
	SimpleVertexShader* fullscreenVS = assets.GetVertexShader("FullscreenVS.cso");

	// Clear all targets and depth buffers
	const float color[4] = { 0, 0, 0, 1 };
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Clear render targets
	for (auto& rt : renderTargetRTVs) context->ClearRenderTargetView(rt.Get(), color);
	const float depth[4] = { 1,0,0,0 };
	context->ClearRenderTargetView(renderTargetRTVs[GBUFFER_DEPTH].Get(), depth);

	// MRTs support up to 8 targets at once
	const int totalTargets = 8;
	ID3D11RenderTargetView* targets[totalTargets] = {};

	// Which render path?
	switch (renderPath)
	{
	case RenderPath::RENDER_PATH_FORWARD: 
		
		// Set up forward rendering MRTs
		targets[0] = renderTargetRTVs[RenderTargetType::SCENE_NO_AMBIENT].Get();
		targets[1] = renderTargetRTVs[RenderTargetType::SCENE_AMBIENT].Get();
		targets[2] = renderTargetRTVs[RenderTargetType::GBUFFER_NORMALS].Get(); // Reusing these targets here since
		targets[3] = renderTargetRTVs[RenderTargetType::GBUFFER_DEPTH].Get();	// we don't need both paths at once
		context->OMSetRenderTargets(totalTargets, targets, depthBufferDSV.Get());

		// Render the scene followed by the sky
		RenderSceneForward(camera);
		sky->Draw(camera);
		break;

	case RenderPath::RENDER_PATH_DEFERRED: 
		
		// Set up gbuffer targets
		targets[0] = renderTargetRTVs[RenderTargetType::GBUFFER_ALBEDO].Get();
		targets[1] = renderTargetRTVs[RenderTargetType::GBUFFER_NORMALS].Get();
		targets[2] = renderTargetRTVs[RenderTargetType::GBUFFER_DEPTH].Get();
		targets[3] = renderTargetRTVs[RenderTargetType::GBUFFER_METAL_ROUGH].Get();
		context->OMSetRenderTargets(totalTargets, targets, depthBufferDSV.Get());

		// Render the scene 
		RenderSceneDeferred(camera); 

		// Draw the lights into the light buffer
		context->OMSetRenderTargets(1, renderTargetRTVs[RenderTargetType::LIGHT_BUFFER].GetAddressOf(), 0);
		RenderLightsDeferred(camera);

		// Final combine before post processing
		targets[0] = renderTargetRTVs[RenderTargetType::SCENE_NO_AMBIENT].Get();
		targets[1] = renderTargetRTVs[RenderTargetType::SCENE_AMBIENT].Get();
		context->OMSetRenderTargets(2, targets, 0);
		fullscreenVS->SetShader();
		SimplePixelShader* combinePS = assets.GetPixelShader("DeferredCombinePS.cso");
		combinePS->SetShader();
		combinePS->SetShaderResourceView("GBufferAlbedo", renderTargetSRVs[RenderTargetType::GBUFFER_ALBEDO]);
		combinePS->SetShaderResourceView("GBufferNormals", renderTargetSRVs[RenderTargetType::GBUFFER_NORMALS]);
		combinePS->SetShaderResourceView("GBufferDepth", renderTargetSRVs[RenderTargetType::GBUFFER_DEPTH]);
		combinePS->SetShaderResourceView("GBufferMetalRough", renderTargetSRVs[RenderTargetType::GBUFFER_METAL_ROUGH]);
		combinePS->SetShaderResourceView("LightBuffer", renderTargetSRVs[RenderTargetType::LIGHT_BUFFER]);
		combinePS->SetShaderResourceView("BrdfLookUpMap", sky->GetBRDFLookUpTexture());
		combinePS->SetShaderResourceView("IrradianceIBLMap", sky->GetIrradianceMap());
		combinePS->SetShaderResourceView("SpecularIBLMap", sky->GetSpecularMap());
		combinePS->SetMatrix4x4("InvViewProj", camera->GetInverseViewProjection());
		combinePS->SetFloat3("CameraPosition", camera->GetTransform()->GetPosition());
		combinePS->SetInt("SpecIBLTotalMipLevels", sky->GetTotalSpecularIBLMipLevels());
		combinePS->SetFloat("IBLIntensity", iblIntensity);
		combinePS->CopyAllBufferData();
		context->Draw(3, 0);

		// Draw the sky
		context->OMSetRenderTargets(1, renderTargetRTVs[RenderTargetType::SCENE_NO_AMBIENT].GetAddressOf(), depthBufferDSV.Get());
		sky->Draw(camera);
		break;
	}

	// About to hit post processing, so wipe out all
	// MRTs so each step below can set their own
	ZeroMemory(targets, sizeof(ID3D11RenderTargetView*) * totalTargets);
	context->OMSetRenderTargets(totalTargets, targets, 0);

	// Set up vertex shader for post processing
	fullscreenVS->SetShader();

	// Render the SSAO results
	{
		// Set up ssao render pass (just need 1 target)
		targets[0] = renderTargetRTVs[RenderTargetType::SSAO_RESULTS].Get();
		for(int i = 1; i < totalTargets; i++)
			targets[i] = 0;
		context->OMSetRenderTargets(totalTargets, targets, 0);

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

		ssaoPS->SetShaderResourceView("Normals", renderTargetSRVs[RenderTargetType::GBUFFER_NORMALS]);
		ssaoPS->SetShaderResourceView("Depths", renderTargetSRVs[RenderTargetType::GBUFFER_DEPTH]);
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
		ps->SetShaderResourceView("SceneColorsNoAmbient", renderTargetSRVs[RenderTargetType::SCENE_NO_AMBIENT]);
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



void Renderer::RenderSceneForward(Camera* camera)
{
	// Collect all per-frame data and copy to GPU
	{
		// vs ----
		vsPerFrameData->ViewMatrix = camera->GetView();
		vsPerFrameData->ProjectionMatrix = camera->GetProjection();
		context->UpdateSubresource(vsPerFrameConstantBuffer.Get(), 0, 0, vsPerFrameData, 0, 0);

		// ps ----
		memcpy(&psPerFrameData->Lights, &lights[0], sizeof(Light) * activeLightCount);
		psPerFrameData->LightCount = activeLightCount;
		psPerFrameData->CameraPosition = camera->GetTransform()->GetPosition();
		psPerFrameData->TotalSpecIBLMipLevels = sky->GetTotalSpecularIBLMipLevels();
		psPerFrameData->AmbientNonPBR = ambientNonPBR;
		psPerFrameData->IBLIntensity = iblIntensity;
		context->UpdateSubresource(psPerFrameConstantBuffer.Get(), 0, 0, psPerFrameData, 0, 0);
		
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
}



void Renderer::RenderSceneDeferred(Camera* camera)
{
	// Collect all per-frame data and copy to GPU
	{
		// vs ----
		vsPerFrameData->ViewMatrix = camera->GetView();
		vsPerFrameData->ProjectionMatrix = camera->GetProjection();
		context->UpdateSubresource(vsPerFrameConstantBuffer.Get(), 0, 0, vsPerFrameData, 0, 0);

		// ps ----
		// None (for now)
	}

	// Make a copy of the renderable list so we can sort it
	std::vector<GameEntity*> toDraw(entities);
	std::sort(toDraw.begin(), toDraw.end(), [](const auto& e1, const auto& e2)
		{
			// Compare pointers to materials
			return e1->GetMaterial() < e2->GetMaterial();
		});

	// Assume we're using the same pixel shader for
	// every single entity, since we're just creating
	// the GBuffer for now
	SimplePixelShader* gbufferPS = Assets::GetInstance().GetPixelShader("GBufferRenderPS.cso");
	gbufferPS->SetShader();
	// If we had PS per frame data, we'd set the constant buffer here

	// Draw all of the entities
	SimpleVertexShader* currentVS = 0;
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

			// Swap out the material's pixel shader for the
			// GBuffer creation shader temporarily, set all
			// data and then swap it back.  Not necessarily
			// the best way to do this, but it works fine!
			SimplePixelShader* matPS = currentMaterial->GetPS();
			currentMaterial->SetPS(gbufferPS);
			currentMaterial->SetPerMaterialDataAndResources(true);
			currentMaterial->SetPS(matPS);

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
}

void Renderer::RenderLightsDeferred(Camera* camera)
{
	// Grab necessary assets
	Assets& assets = Assets::GetInstance();
	SimplePixelShader* dirPS = assets.GetPixelShader("DeferredDirectionalLightPS.cso");
	SimpleVertexShader* dirVS = assets.GetVertexShader("DeferredDirectionalLightVS.cso");
	SimplePixelShader* pointPS = assets.GetPixelShader("DeferredPointLightPS.cso");
	SimpleVertexShader* pointVS = assets.GetVertexShader("DeferredPointLightVS.cso");
	Mesh* sphereMesh = assets.GetMesh("Models\\sphere.obj");

	// Set GBuffer SRVs once
	// Note: Making the assumption that all deferred "light" shaders
	// expect the same textures at the same place
	ID3D11ShaderResourceView* gbuffer[4] = {};
	gbuffer[0] = renderTargetSRVs[RenderTargetType::GBUFFER_ALBEDO].Get();
	gbuffer[1] = renderTargetSRVs[RenderTargetType::GBUFFER_NORMALS].Get();
	gbuffer[2] = renderTargetSRVs[RenderTargetType::GBUFFER_DEPTH].Get();
	gbuffer[3] = renderTargetSRVs[RenderTargetType::GBUFFER_METAL_ROUGH].Get();
	context->PSSetShaderResources(0, 4, gbuffer);

	// Set the blend state so that light results add together
	context->OMSetBlendState(deferredAdditiveBlendState.Get(), 0, 0xFFFFFFFF);

	// Loop through active lights and render each
	int currentLightType = -1;
	for (unsigned int i = 0; i < activeLightCount; i++)
	{
		// Check the light type
		Light light = lights[i];
		switch (light.Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:

			// Minimize state swaps by checking the most recent light type
			if (currentLightType != LIGHT_TYPE_DIRECTIONAL)
			{
				// Swap states
				context->OMSetDepthStencilState(deferredDirectionalLightDepthState.Get(), 0);
				context->RSSetState(0);

				// Set up common shader data
				dirVS->SetShader();

				dirPS->SetShader();
				dirPS->SetMatrix4x4("InvViewProj", camera->GetInverseViewProjection());
				dirPS->SetFloat3("CameraPosition", camera->GetTransform()->GetPosition());
				dirPS->CopyBufferData("perFrame");

				// Remember light type
				currentLightType = light.Type;
			}

			// Per-light data
			dirPS->SetData("ThisLight", (void*)(&light), sizeof(Light));
			dirPS->CopyBufferData("perLight");

			// Actually draw the light (fullscreen triangle for directional light)
			context->Draw(3, 0);

			break;

		case LIGHT_TYPE_POINT:

			// Minimize state swaps by checking the most recent light type
			if (currentLightType != LIGHT_TYPE_POINT)
			{
				// Swap states
				context->OMSetDepthStencilState(deferredPointLightDepthState.Get(), 0);
				context->RSSetState(deferredCullFrontRasterState.Get());

				// Set up common shader data
				pointVS->SetShader();
				pointVS->SetMatrix4x4("View", camera->GetView());
				pointVS->SetMatrix4x4("Projection", camera->GetProjection());
				pointVS->SetFloat3("CameraPosition", camera->GetTransform()->GetPosition());
				pointVS->CopyBufferData("perFrame");

				pointPS->SetShader();
				pointPS->SetMatrix4x4("InvViewProj", camera->GetInverseViewProjection());
				pointPS->SetFloat3("CameraPosition", camera->GetTransform()->GetPosition());
				pointPS->SetFloat("WindowWidth", windowWidth);
				pointPS->SetFloat("WindowHeight", windowHeight);
				pointPS->CopyBufferData("perFrame");

				// Remember light type
				currentLightType = light.Type;
			}

			// Calculate a world matrix for the sphere based on the light's position and radius
			{
				float rad = light.Range * 2; // This sphere model has a radius of 0.5, so double the scale
				XMFLOAT4X4 world;
				XMMATRIX trans = XMMatrixTranslationFromVector(XMLoadFloat3(&light.Position));
				XMMATRIX sc = XMMatrixScaling(rad, rad, rad);
				XMStoreFloat4x4(&world, sc * trans);
			
				// Per-light data
				pointVS->SetMatrix4x4("World", world);
				pointVS->CopyBufferData("perLight");

				pointPS->SetData("ThisLight", (void*)(&light), sizeof(Light));
				pointPS->CopyBufferData("perLight");
			}

			// Draw the point light (sphere)
			sphereMesh->SetBuffersAndDraw(context);

			break;

		case LIGHT_TYPE_SPOT: break; // Not implemented in this demo!
		}

	}

	// Reset states
	context->RSSetState(0); 
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0); // Double check this one?
	
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
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::GBUFFER_ALBEDO], renderTargetSRVs[RenderTargetType::GBUFFER_ALBEDO]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::GBUFFER_NORMALS], renderTargetSRVs[RenderTargetType::GBUFFER_NORMALS], DXGI_FORMAT_R16G16B16A16_FLOAT);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::GBUFFER_DEPTH], renderTargetSRVs[RenderTargetType::GBUFFER_DEPTH], DXGI_FORMAT_R32_FLOAT);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::GBUFFER_METAL_ROUGH], renderTargetSRVs[RenderTargetType::GBUFFER_METAL_ROUGH]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::LIGHT_BUFFER], renderTargetSRVs[RenderTargetType::LIGHT_BUFFER], DXGI_FORMAT_R16G16B16A16_FLOAT);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_NO_AMBIENT], renderTargetSRVs[RenderTargetType::SCENE_NO_AMBIENT]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SCENE_AMBIENT], renderTargetSRVs[RenderTargetType::SCENE_AMBIENT]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_RESULTS], renderTargetSRVs[RenderTargetType::SSAO_RESULTS]);
	CreateRenderTarget(windowWidth, windowHeight, renderTargetRTVs[RenderTargetType::SSAO_BLUR], renderTargetSRVs[RenderTargetType::SSAO_BLUR]);
}

unsigned int Renderer::GetActiveLightCount() { return activeLightCount; }
void Renderer::SetActiveLightCount(unsigned int count){	activeLightCount = min(count, MAX_LIGHTS); }

void Renderer::SetPointLightsVisible(bool visible) { pointLightsVisible = visible; }
bool Renderer::GetPointLightsVisible() { return pointLightsVisible; }

void Renderer::SetRenderPath(RenderPath path) { renderPath = path; }
RenderPath Renderer::GetRenderPath() { return renderPath; }

void Renderer::SetSSAOEnabled(bool enabled) { ssaoEnabled = enabled; }
bool Renderer::GetSSAOEnabled() { return ssaoEnabled; }

void Renderer::SetSSAORadius(float radius) { ssaoRadius = radius; }
float Renderer::GetSSAORadius() { return ssaoRadius; }

void Renderer::SetSSAOSamples(int samples) { ssaoSamples = max(0, min(samples, ARRAYSIZE(ssaoOffsets))); }
int Renderer::GetSSAOSamples() { return ssaoSamples; }

void Renderer::SetSSAOOutputOnly(bool ssaoOnly) { ssaoOutputOnly = ssaoOnly; }
bool Renderer::GetSSAOOutputOnly() { return ssaoOutputOnly; }

void Renderer::SetIBLIntensity(float intensity) { iblIntensity = intensity; }
float Renderer::GetIBLIntensity() { return iblIntensity; }

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Renderer::GetRenderTargetSRV(RenderTargetType type)
{ 
	if (type < 0 || type >= RenderTargetType::RENDER_TARGET_TYPE_COUNT)
		return 0;

	return renderTargetSRVs[type];
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
