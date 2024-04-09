
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Assets.h"

#include "WICTextureLoader.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#ifndef RandomRange
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min
#endif

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,			// The application's handle
		L"DirectX Game",	// Text for the window's title bar (as a wide-character string)
		1280,				// Width of the window's client area
		720,				// Height of the window's client area
		false,				// Sync the framerate to the monitor refresh? (lock framerate)
		true),				// Show extra stats (fps) in title bar?
	ambientColor(0, 0, 0), // Ambient is zero'd out since it's not physically-based
	lightCount(3)
{
	// Seed random
	srand((unsigned int)time(0));

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object

	// Delete singletons
	delete& Assets::GetInstance();

	// IMGUI
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());
	ImGui::StyleColorsDark();

	// Asset loading and entity creation
	LoadAssetsAndCreateEntities();

	// Set up lights
	lightCount = 3;
	GenerateLights();

	// Set initial graphics API state
	//  - These settings persist until we change them
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// Create the camera
	camera = std::make_shared<Camera>(
		0.0f, 1.0f, -15.0f, // Position
		5.0f,				// Move speed
		0.002f,				// Look speed
		XM_PIDIV4,			// Field of view
		(float)windowWidth / windowHeight,  // Aspect ratio
		0.01f,				// Near clip
		100.0f,				// Far clip
		CameraProjectionType::Perspective);
}


// --------------------------------------------------------
// Load all assets and create materials, entities, etc.
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	Assets& assets = Assets::GetInstance();
	assets.Initialize(L"../../../../Assets/", L"./", device, context, true, true);

	// Create a sampler state for texture sampling options
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());


	// Create the sky (loading custom shaders in-line below)
	sky = std::make_shared<Sky>(
		FixPath(L"../../../../Assets/Skies/Night Moon/right.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/left.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/up.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/down.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/front.png").c_str(),
		FixPath(L"../../../../Assets/Skies/Night Moon/back.png").c_str(),
		assets.GetMesh(L"Models/cube"),
		assets.GetVertexShader(L"SkyVS"),
		assets.GetPixelShader(L"SkyPS"),
		sampler,
		device,
		context);

	// Grab shaders needed below
	std::shared_ptr<SimpleVertexShader> vertexShader = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> pixelShader = assets.GetPixelShader(L"PixelShaderPBR");

	// Create basic materials
	std::shared_ptr<Material> paintMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/paint_albedo"));
	paintMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/paint_normals"));
	paintMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/paint_roughness"));
	paintMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/paint_metal"));

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/scratched_albedo"));
	scratchedMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/scratched_normals"));
	scratchedMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/scratched_roughness"));
	scratchedMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/scratched_metal"));

	std::shared_ptr<Material> woodMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/wood_albedo"));
	woodMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/wood_normals"));
	woodMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/wood_roughness"));
	woodMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/wood_metal"));


	// === Create the scene ===
	std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(assets.GetMesh(L"Models/sphere"), scratchedMat);
	sphere->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(sphere);

	std::shared_ptr<GameEntity> helix = std::make_shared<GameEntity>(assets.GetMesh(L"Models/helix"), paintMat);
	entities.push_back(helix);

	std::shared_ptr<GameEntity> cube = std::make_shared<GameEntity>(assets.GetMesh(L"Models/cube"), woodMat);
	cube->GetTransform()->SetPosition(5, 0, 0);
	cube->GetTransform()->SetScale(2, 2, 2);
	entities.push_back(cube);


	// Grab loaded particle resources
	std::shared_ptr<SimpleVertexShader> particleVS = assets.GetVertexShader(L"ParticleVS");
	std::shared_ptr<SimplePixelShader> particlePS = assets.GetPixelShader(L"ParticlePS");

	// Create particle materials
	std::shared_ptr<Material> fireParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	fireParticle->AddSampler("BasicSampler", sampler);
	fireParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/Black/fire_01"));

	std::shared_ptr<Material> twirlParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	twirlParticle->AddSampler("BasicSampler", sampler);
	twirlParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/Black/twirl_02"));

	std::shared_ptr<Material> starParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	starParticle->AddSampler("BasicSampler", sampler);
	starParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/Black/star_04"));

	std::shared_ptr<Material> animParticle = std::make_shared<Material>(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	animParticle->AddSampler("BasicSampler", sampler);
	animParticle->AddTextureSRV("Particle", assets.GetTexture(L"Textures/Particles/flame_animated"));

	
	// Particle states ====

	// A depth state for the particles
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Turns off depth writing
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	device->CreateDepthStencilState(&dsDesc, particleDepthState.GetAddressOf());

	// Blend for particles (additive)
	D3D11_BLEND_DESC blend = {};
	blend.AlphaToCoverageEnable = false;
	blend.IndependentBlendEnable = false;
	blend.RenderTarget[0].BlendEnable = true;
	blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; // Still respect pixel shader output alpha
	blend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blend, particleBlendState.GetAddressOf());

	// Debug rasterizer state for particles
	D3D11_RASTERIZER_DESC rd = {};
	rd.CullMode = D3D11_CULL_BACK;
	rd.DepthClipEnable = true;
	rd.FillMode = D3D11_FILL_WIREFRAME;
	device->CreateRasterizerState(&rd, particleDebugRasterState.GetAddressOf());

	// Flame thrower
	emitters.push_back(std::make_shared<Emitter>(
		device,
		fireParticle,
		160,							// Max particles
		30,								// Particles per second
		5.0f,							// Particle lifetime
		0.1f,							// Start size
		4.0f,							// End size
		false,
		XMFLOAT4(1, 0.1f, 0.1f, 0.7f),	// Start color
		XMFLOAT4(1, 0.6f, 0.1f, 0),		// End color
		XMFLOAT3(2, 0, 0),				// Emitter position
		XMFLOAT3(0.1f, 0.1f, 0.1f),		// Position randomness range
		XMFLOAT2(-2, 2),
		XMFLOAT2(-2, 2),				// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(-2, 2, 0),				// Start velocity
		XMFLOAT3(0.2f, 0.2f, 0.2f),		// Velocity randomness range
		XMFLOAT3(0, -1, 0)));			// Constant acceleration
		

	// Erratic swirly portal
	emitters.push_back(std::make_shared<Emitter>(
		device,
		twirlParticle,
		45,								// Max particles
		20,								// Particles per second
		2.0f,							// Particle lifetime
		3.0f,							// Start size
		2.0f,							// End size
		false,
		XMFLOAT4(0.2f, 0.1f, 0.1f, 0.0f),// Start color
		XMFLOAT4(0.2f, 0.7f, 0.1f, 1.0f),// End color
		XMFLOAT3(3.5f, 3.5f, 0),		// Emitter position
		XMFLOAT3(0, 0, 0),				// Position randomness range
		XMFLOAT2(-5, 5),
		XMFLOAT2(-5, 5),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, 0, 0),				// Start velocity
		XMFLOAT3(0, 0, 0),				// Velocity randomness range
		XMFLOAT3(0, 0, 0)));			// Constant acceleration

	// Falling star field
	emitters.push_back(std::make_shared<Emitter>(
		device,
		starParticle,
		250,							// Max particles
		100,							// Particles per second
		2.0f,							// Particle lifetime
		2.0f,							// Start size
		0.0f,							// End size
		false,
		XMFLOAT4(0.1f, 0.2f, 0.5f, 0.0f),// Start color
		XMFLOAT4(0.1f, 0.1f, 0.3f, 3.0f),// End color (ending with high alpha so we hit 1.0 sooner)
		XMFLOAT3(-2.5f, -1, 0),			// Emitter position
		XMFLOAT3(1, 0, 1),				// Position randomness range
		XMFLOAT2(0, 0),
		XMFLOAT2(-3, 3),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, 0, 0),				// Start velocity
		XMFLOAT3(0.1f, 0, 0.1f),		// Velocity randomness range
		XMFLOAT3(0, -2, 0)));			// Constant acceleration


	// Animated fire texture
	emitters.push_back(std::make_shared<Emitter>(
		device,
		animParticle, 
		5,						// Max particles
		2,						// Particles per second
		2.0f,					// Particle lifetime
		2.0f,					// Start size
		2.0f,					// End size
		false,
		XMFLOAT4(1, 1, 1, 1),	// Start color
		XMFLOAT4(1, 1, 1, 0),	// End color
		XMFLOAT3(2, -2, 0),		// Emitter position
		XMFLOAT3(0, 0, 0),		// Position randomness range
		XMFLOAT2(-2, 2),
		XMFLOAT2(-2, 2),	// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, 0, 0),		// Start velocity
		XMFLOAT3(0, 0, 0),		// Velocity randomness range
		XMFLOAT3(0, 0, 0),		// Constant acceleration
		8,
		8));
}


// --------------------------------------------------------
// Generates the lights in the scene: 3 directional lights
// and many random point lights.
// --------------------------------------------------------
void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-10.0f, 10.0f), RandomRange(-5.0f, 5.0f), RandomRange(-10.0f, 10.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}

}



// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update our projection matrix to match the new aspect ratio
	if (camera) camera->UpdateProjectionMatrix((float)windowWidth / windowHeight);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Since Init() takes a while, the first deltaTime
	// ends up being a massive number, which ends up emitting
	// a ton of particles.  Skipping the very first frame!
	static bool firstFrame = true; // Only ever initialized once due to static
	if (firstFrame) { deltaTime = 0.0f; firstFrame = false; }

	// Get the input instance once
	Input& input = Input::GetInstance();

	// Update the camera
	camera->Update(deltaTime);

	// Update all emitters
	for (auto& e : emitters)
		e->Update(deltaTime, totalTime);

	// Create the UI during update!
	CreateUI(deltaTime);

	// Check various keys
	if (input.KeyDown(VK_ESCAPE)) Quit();
	if (input.KeyPress(VK_TAB)) GenerateLights();
}


void Game::CreateUI(float dt)
{
	// IMGUI
	{
		// Feed fresh input data to ImGui
		ImGuiIO& io = ImGui::GetIO();
		io.DeltaTime = dt;
		io.DisplaySize.x = (float)this->windowWidth;
		io.DisplaySize.y = (float)this->windowHeight;

		// Reset the frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Determine new input capture
		Input& input = Input::GetInstance();
		input.SetKeyboardCapture(io.WantCaptureKeyboard);
		input.SetMouseCapture(io.WantCaptureMouse);
	}

	// Combined into a single window
	ImGui::Begin("Debug");

	// Showing the demo window?
	{
		static bool showDemoWindow = false;
		if (ImGui::Button("Show Demo Window"))
			showDemoWindow = !showDemoWindow;

		if (showDemoWindow)
			ImGui::ShowDemoWindow();
	}

	// Emitters
	if (ImGui::CollapsingHeader("Particle Emitters"))
	{
		ImGui::Indent(10.0f);
		for (int i = 0; i < emitters.size(); i++)
		{
			UIEmitter(emitters[i], i);
		}
		ImGui::Indent(-10.0f);
	}

	ImGui::End();
}


void Game::UIEmitter(std::shared_ptr<Emitter> emitter, int index)
{
	std::string indexStr = std::to_string(index);

	std::string nodeName = "Emitter " + indexStr;
	if (ImGui::TreeNode(nodeName.c_str()))
	{
		ImGui::Indent(10.0f);

		// Emission
		ImGui::Text("Emission & Lifetime");
		{
			ImGui::Indent(5.0f);

			int maxPart = emitter->GetMaxParticles();
			if (ImGui::DragInt("Max Particles", &maxPart, 1.0f, 1, 2000))
				emitter->SetMaxParticles(maxPart);

			int partPerSec = emitter->GetParticlesPerSecond();
			if (ImGui::DragInt("Particles Per Second", &partPerSec, 1.0f, 1, 2000))
				emitter->SetParticlesPerSecond(partPerSec);

			ImGui::SliderFloat("Lifetime", &emitter->lifetime, 0.1f, 25.0f);

			ImGui::Indent(-5.0f);
		}

		// Overall movement data
		ImGui::Spacing();
		ImGui::Text("Movement");
		{
			ImGui::Indent(5.0f);

			XMFLOAT3 pos = emitter->GetTransform()->GetPosition();
			if (ImGui::DragFloat3("Emitter Position", &pos.x, 0.05f))
				emitter->GetTransform()->SetPosition(pos);
			ImGui::DragFloat3("Position Randomness", &emitter->positionRandomRange.x, 0.05f);

			ImGui::DragFloat3("Starting Velocity", &emitter->startVelocity.x, 0.05f);
			ImGui::DragFloat3("Velocity Randomness", &emitter->velocityRandomRange.x, 0.05f);

			ImGui::DragFloat3("Acceleration", &emitter->emitterAcceleration.x, 0.05f);
			ImGui::Indent(-5.0f);
		}

		// Visuals
		ImGui::Spacing();
		ImGui::Text("Visuals");
		{
			ImGui::Indent(5.0f);
			ImGui::ColorEdit4("Starting Color", &emitter->startColor.x);
			ImGui::ColorEdit4("Ending Color", &emitter->endColor.x);

			ImGui::SliderFloat("Starting Size", &emitter->startSize, 0.0f, 10.0f);
			ImGui::SliderFloat("Ending Size", &emitter->endSize, 0.0f, 10.0f);

			ImGui::DragFloatRange2(
				"Rotation Start Range",
				&emitter->rotationStartMinMax.x,
				&emitter->rotationStartMinMax.y,
				0.01f);

			ImGui::DragFloatRange2(
				"Rotation End Range",
				&emitter->rotationEndMinMax.x,
				&emitter->rotationEndMinMax.y,
				0.01f);

			ImGui::Checkbox("Constrain Rotation on Y", &emitter->constrainYAxis);

			if (emitter->IsSpriteSheet())
			{
				ImGui::SliderFloat("Sprite Sheet Animation Speed", &emitter->spriteSheetSpeedScale, 0.0f, 10.0f);
			}

			ImGui::Indent(-5.0f);
		}

		// Clean up this node
		ImGui::Indent(-10.0f);
		ImGui::TreePop();
	}
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Frame START
	// - These things should happen ONCE PER FRAME
	// - At the beginning of Game::Draw() before drawing *anything*
	{
		// Clear the back buffer (erases what's on the screen)
		const float bgColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black
		context->ClearRenderTargetView(backBufferRTV.Get(), bgColor);

		// Clear the depth buffer (resets per-pixel occlusion information)
		context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	// Loop through the game entities in the current scene and draw
	for (auto& e : entities)
	{
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightCount);

		// Draw one entity
		e->Draw(context, camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw all emitters
	DrawParticles(totalTime);

	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Draw the UI after everything else
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Present the back buffer to the user
		//  - Puts the results of what we've drawn onto the window
		//  - Without this, the user never sees anything
		bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
		swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Must re-bind buffers after presenting, as they become unbound
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
	}
}

void Game::DrawParticles(float totalTime)
{
	// Particle drawing =============
	{

		// Particle states
		context->OMSetBlendState(particleBlendState.Get(), 0, 0xffffffff);	// Additive blending
		context->OMSetDepthStencilState(particleDepthState.Get(), 0);		// No depth WRITING

		// Draw all of the emitters
		for (auto& e : emitters)
		{
			e->Draw(context, camera, totalTime);
		}

		// Should we also draw them in wireframe?
		if (Input::GetInstance().KeyDown('C'))
		{
			context->RSSetState(particleDebugRasterState.Get());
			for (auto& e : emitters)
			{
				e->Draw(context, camera, totalTime);
			}
		}

		// Reset to default states for next frame
		context->OMSetBlendState(0, 0, 0xffffffff);
		context->OMSetDepthStencilState(0, 0);
		context->RSSetState(0);
	}
}
